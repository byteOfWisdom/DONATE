#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <fcntl.h>

std::string out_fifo(std::string& name) {
	return "/tmp/donate/" + name + "_pub.fifo";
}

std::string in_fifo(std::string& name) {
	return "/tmp/donate/" + name + "_sub.fifo";
}

void make_proc_endpoints(std::string& name) {
	auto out_name = out_fifo(name);
	auto in_name = in_fifo(name);
	mkfifo(out_name.data(), 777);
	mkfifo(in_name.data(), 777);
}

template <typename T>
struct option {
	bool some;
	T thing;

	option() {
		this->some = false;
	};

	option(T thing) {
		this->some = true;
		this->thing = thing;
	}
};

struct message_t {
	std::string tag, content;
};

struct fifo_buffer {
	const static uint buffsize = 10;
	message_t messages[buffsize];
	bool full[buffsize];
	std::mutex lock;

	~fifo_buffer() = default;

	bool write(message_t);
	option<message_t> read_available();
};

option<message_t> fifo_buffer::read_available() {
	const std::lock_guard<std::mutex> lg(this->lock);
	for (uint i = 0; i < this->buffsize; ++ i) {
		if (this->full[i]) {
			this->full[i] = false;
			return option<message_t>(messages[i]);
		}
	}
	return option<message_t>();
}

bool fifo_buffer::write(message_t msg) {
	const std::lock_guard<std::mutex> lg(this->lock);
	for (uint i = 0; i < this->buffsize; ++ i) {
		if (!this->full[i]) {
			this->full[i] = true;
			this->messages[i] = msg;
			return true;
		}
	}
	return false;
}


message_t parse(std::string& message) {
	message_t res;
	if (message.length() == 0) return res;
	if (message[0] != '\\') {
		res.content = message;
		return res;
	}

	uint i = 1;
	for (; i < message.length(); ++i) {
		if (message[i] == '\\') break;
		res.tag += message[i];
	}

	res.content = message.substr(i + 1, message.length() - i);
	return res;
}

class sub_list {
	public:
	std::span<fifo_buffer*> get(std::string& tag);
	bool not_tag(std::string&);
	sub_list();
	fifo_buffer* subscribe(std::vector<std::string>&);
	std::mutex lock;

	private:
	std::map<std::string, uint> subscriptions;
	std::vector<std::vector<fifo_buffer*>> fifo_buffers;
};

sub_list::sub_list() {
	this->subscriptions = std::map<std::string, uint>();
	this->fifo_buffers = std::vector<std::vector<fifo_buffer*>>();
}

fifo_buffer* sub_list::subscribe(std::vector<std::string>& tag_list) {
	auto buffer = new fifo_buffer();

	for (auto tag : tag_list){
		if (!this->subscriptions.contains(tag)) {
			this->subscriptions.insert({tag, this->fifo_buffers.size()});
			this->fifo_buffers.push_back(std::vector<fifo_buffer*>());
		}

		uint buff_index = this->subscriptions[tag];
		this->fifo_buffers[buff_index].push_back(buffer);
	}

	return buffer;
}


bool sub_list::not_tag(std::string& tag) {
	return !this->subscriptions.contains(tag);
}

std::span<fifo_buffer*> sub_list::get(std::string& tag) {
	if (this->not_tag(tag)) {
		return std::span<fifo_buffer*>();
	}

	return this->fifo_buffers[this->subscriptions[tag]];
}


void publish_messages(std::string& from, sub_list& sub_info) {
	std::ifstream input(out_fifo(from));
	std::string line;

	while (std::getline(input, line)) {
		auto message = parse(line);
		if (sub_info.not_tag(message.tag)) {
			printf("[%s] %s\n", from.data(), line.data());
		}
		for (auto subscriber : sub_info.get(message.tag)) {
			//printf("writing to fifo_buffer\n");
			subscriber->write(message);
		}
	}
}

void write_incoming_subs(std::string name, fifo_buffer* sub_fifo) {
//	FILE* handle = fopen(in_fifo(name).data(), "w");
	int handle = open(in_fifo(name).data(), O_WRONLY);

	while (1) {
		auto message = sub_fifo->read_available();
		if (message.some) {
			//printf("writing message to proc\n");
			std::string s_msg = "\\" + message.thing.tag + "\\" + message.thing.content + "\n";
			write(handle, s_msg.data(), s_msg.length() + 1);
		}
	}
}

struct proc_info{
	std::string name;
	std::vector<std::string> subs;
};

void launch(proc_info p, sub_list& master_sublist) {
	const auto lg = new std::lock_guard<std::mutex>(master_sublist.lock);
	make_proc_endpoints(p.name);
	auto sub_fifo = master_sublist.subscribe(p.subs);
	delete lg;
	auto write_subs_fn = [=](){write_incoming_subs(p.name, sub_fifo);};
	auto read_pubs_fn = [&](){publish_messages(p.name, master_sublist);};
	auto writer = std::thread(write_subs_fn);
	auto reader = std::thread(read_pubs_fn); //thread
	writer.join();
	reader.join();
}

int main(void) {
	sub_list sub_master;
	proc_info test{"test", {"data"}};
	proc_info spam{"spam", {}};
	auto test_handle = std::thread([&](){launch(test, sub_master);});
	auto spam_handle = std::thread([&](){launch(spam, sub_master);});
	printf("launched all processes\n");
	test_handle.join();
	spam_handle.join();

	return 0;
}
