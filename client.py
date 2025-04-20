import socket
import numpy as np

def generate_matrices(n):
    matrix1 = np.random.randint(1, 10, (n, n))
    matrix2 = np.random.randint(1, 10, (n, n))
    return matrix1, matrix2

def send_data(matrix1, matrix2, threads):
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(('localhost', 12345))

    client_socket.sendall(b'HELLO')
    response = client_socket.recv(1024)
    print(response.decode())

    data = f"{matrix1.tolist()},{matrix2.tolist()},{threads}"
    client_socket.sendall(data.encode())
    response = client_socket.recv(1024)
    print(response.decode())

    client_socket.close()

if __name__ == "__main__":
    n = int(input("Введіть розмір матриці (n): "))
    threads = int(input("Введіть кількість потоків: "))
    matrix1, matrix2 = generate_matrices(n)
    print("Матриця 1:")
    print(matrix1)
    print("Матриця 2:")
    print(matrix2)
    send_data(matrix1, matrix2, threads)