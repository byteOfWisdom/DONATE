use std::env;
use std::io::{self, BufRead, BufReader};
use std::os::unix::net::UnixStream;
use std::thread;

fn print_stream(name: String) {
    let mut stream = BufReader::new(UnixStream::connect(name).unwrap());

    loop {
        let mut line = String::new();
        match stream.read_line(&mut line) {
            Ok(_) => {}
            Err(_) => break,
        };
        print!("{}", line);
    }

    println!("exiting stdin thread???");
}

fn print_stdin() {
    let mut stream = BufReader::new(io::stdin());
    loop {
        let mut line = String::new();
        match stream.read_line(&mut line) {
            Ok(_) => {}
            Err(_) => break,
        };
        print!("{}", line);
    }

    println!("exiting stdin thread???");
}

fn main() {
    let argv: Vec<String> = env::args().collect();
    let sock_name = match argv.len() {
        2 => String::from(argv[1].clone().trim()),
        _ => String::from(""),
    };

    let sock_path = format!("/tmp/donate_{}.sock", sock_name);
    let sock_thread = thread::spawn(move || print_stream(sock_path.clone()));
    let stdin_thread = thread::spawn(|| print_stdin());
    sock_thread.join().unwrap();
    stdin_thread.join().unwrap();
}
