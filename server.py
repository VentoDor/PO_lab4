import socket
import threading
import numpy as np
from concurrent.futures import ThreadPoolExecutor

def add_matrices_parallel(matrix1, matrix2, threads):
    result = np.zeros_like(matrix1)
    chunk_size = len(matrix1) // threads

    def add_chunk(start, end):
        result[start:end] = matrix1[start:end] + matrix2[start:end]

    with ThreadPoolExecutor(max_workers=threads) as executor:
        for i in range(threads):
            start = i * chunk_size
            end = (i + 1) * chunk_size if i != threads - 1 else len(matrix1)
            executor.submit(add_chunk, start, end)

    return result

def handle_client(client_socket):
    command = client_socket.recv(1024).decode()
    if command == "HELLO":
        client_socket.sendall(b"CONNECTED")

    data = client_socket.recv(4096).decode()
    matrix1, matrix2, threads = data.split(',')
    matrix1 = np.array(eval(matrix1))
    matrix2 = np.array(eval(matrix2))
    threads = int(threads)

    result = add_matrices_parallel(matrix1, matrix2, threads)

    client_socket.sendall(str(result.tolist()).encode())
    client_socket.close()

def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('localhost', 12345))
    server.listen(5)
    print("Server started")

    while True:
        client_socket, addr = server.accept()
        print(f"Connection from {addr}")
        client_handler = threading.Thread(target=handle_client, args=(client_socket,))
        client_handler.start()

if __name__ == "__main__":
    start_server()