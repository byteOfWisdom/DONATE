use std::env;
use std::io::{self, BufReader, Read, Write};
use std::os::unix::net::UnixStream;

fn main() {
    let argv: Vec<String> = env::args().collect();
    let sock_name = match argv.len() {
        2 => String::from(argv[1].clone().trim()),
        _ => String::from(""),
    };

    let mut socket = UnixStream::connect(format!("/tmp/donate_{}.sock", sock_name)).unwrap();
    let mut stream = BufReader::new(io::stdin());

    loop {
        let mut m_buff: [u8; 1024] = [0; 1024];
        match stream.read(&mut m_buff) {
            Ok(n) => {
                if n > 0 {
                    socket.write(&m_buff).unwrap();
                }
            }
            Err(_) => {}
        }
    }
}
