import subprocess
import threading

def main():
    donate = threading.Thread(target=subprocess.call, args=("../build/donate",))
    #meta_handle = open("/tmp/donate/meta.fifo", 'r')


    while True:
        command = input(">> ")
        print(command)


if __name__ == "__main__":
    main()
