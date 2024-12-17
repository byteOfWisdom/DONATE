use core::str;
use crossbeam_channel::{unbounded, Receiver, Sender};
use std::env;
use std::fs;
use std::io::{Read, Write};
use std::os::unix::net::{UnixListener, UnixStream};
use std::thread;
use std::time::Duration;

fn handle_client(mut stream: UnixStream, pub_to: Sender<String>, recv_from: Receiver<String>) {
    stream.set_nonblocking(true).unwrap();

    let mut most_recent = String::new();
    let mut stream_prio = true;

    loop {
        let mut m_buff: [u8; 1024] = [0; 1024];
        if stream_prio {
            match stream.read(&mut m_buff) {
                Ok(n) => {
                    if n > 0 {
                        //println!("got data from client");
                        most_recent = String::from(str::from_utf8(&m_buff).unwrap());
                        pub_to.send(most_recent.clone()).unwrap();
                    }
                }
                Err(_) => {
                    stream_prio = false;
                }
            }
        } else {
            match recv_from.try_recv() {
                Ok(inc) => {
                    if inc != most_recent {
                        //println!("wrting data to socket");
                        while match stream.write_all(&inc.as_bytes()) {
                            Ok(_) => false,
                            Err(_) => true,
                        } {}
                    }
                }
                Err(_) => {
                    stream_prio = true;
                }
            }
        }
    }
}

fn mux(pubs: Receiver<String>, outs: Vec<Sender<String>>, terminate: Receiver<bool>) {
    loop {
        match terminate.try_recv() {
            Ok(_) => break,
            Err(_) => {}
        };

        let msg = match pubs.recv_timeout(Duration::from_millis(100)) {
            Ok(m) => m,
            Err(_) => continue,
        };
        outs.iter().for_each(|x| x.send(msg.clone()).unwrap());
    }
    println!("restarting muxer");
}

fn main() {
    let argv: Vec<String> = env::args().collect();
    let sock_name = match argv.len() {
        2 => String::from(argv[1].clone().trim()),
        _ => String::from(""),
    };

    let sock_path = format!("/tmp/donate_{}.sock", sock_name);
    match fs::remove_file(&sock_path) {
        Ok(_) => {}
        Err(_) => {}
    };

    let listener = UnixListener::bind(sock_path).unwrap();
    let (send_mux, recv_mux) = unbounded();
    let (terminate_send, terminate_recv) = unbounded();
    let mut all_subs = Vec::new();

    let usable_subs = all_subs.clone();
    let usable_rmux = recv_mux.clone();
    let usable_kill = terminate_recv.clone();
    let mut mux_handle = thread::spawn(move || mux(usable_rmux, usable_subs, usable_kill));

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("new connection!");
                let (mux_pub, from_mux) = unbounded();
                all_subs.push(mux_pub);
                let instance_sender = send_mux.clone();
                thread::spawn(|| handle_client(stream, instance_sender, from_mux));

                // stop and restart with new channel set
                terminate_send.send(true).unwrap();
                let usable_subs = all_subs.clone();
                let usable_rmux = recv_mux.clone();
                let usable_kill = terminate_recv.clone();
                while !mux_handle.is_finished() {}
                mux_handle.join().unwrap();
                mux_handle = thread::spawn(move || mux(usable_rmux, usable_subs, usable_kill));
            }
            Err(err) => {
                println!("Error: {}", err);
                break;
            }
        }
    }
}
