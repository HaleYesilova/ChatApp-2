import socket
import threading
import sys
import time
from datetime import datetime

SERVER = "127.0.0.1"
PORT = 8080

class ChatClient:
    def __init__(self):
        self.nickname = input("Kullanıcı adınızı girin: ")
        self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connected = False

    def connect(self):
        try:
            self.client.connect((SERVER, PORT))
            self.connected = True
            print("Sunucuya bağlanıldı!")
            return True
        except Exception as e:
            print(f"Bağlantı hatası: {e}")
            return False

    def receive_messages(self):
        while self.connected:
            try:
                message = self.client.recv(1024).decode('utf-8')
                if message == 'NICK':
                    self.client.send(self.nickname.encode('utf-8'))
                else:
                    print(f"\n{message}")
            except:
                print("Sunucu bağlantısı kesildi!")
                self.connected = False
                break

    def write_messages(self):
        while self.connected:
            try:
                message = input('')
                if message.lower() == '/quit':
                    self.connected = False
                    self.client.close()
                    break
                elif message.lower() == '/help':
                    self.show_help()
                    continue
                elif message.lower() == '/list':
                    self.client.send('/list'.encode('utf-8'))
                    continue
                elif message.startswith('/msg '):
                    timestamp = datetime.now().strftime("%H:%M")
                    full_message = f'/msg {message[5:]}'
                    self.client.send(full_message.encode('utf-8'))
                    continue
                
                timestamp = datetime.now().strftime("%H:%M")
                full_message = f'[{timestamp}] {self.nickname}: {message}'
                self.client.send(full_message.encode('utf-8'))
            except Exception as e:
                print(f"Mesaj gönderme hatası: {e}")
                self.connected = False
                break

    def show_help(self):
        help_text = """
        Kullanılabilir komutlar:
        /help - Bu yardım mesajını gösterir
        /list - Aktif kullanıcıları listeler
        /msg <kullanıcı> <mesaj> - Özel mesaj gönderir
        /quit - Sohbetten çıkar
        """
        print(help_text)

    def start(self):
        if not self.connect():
            return

        print("Komutları görmek için /help yazın")
        
        receive_thread = threading.Thread(target=self.receive_messages)
        receive_thread.daemon = True
        receive_thread.start()

        write_thread = threading.Thread(target=self.write_messages)
        write_thread.daemon = True
        write_thread.start()

        while self.connected:
            time.sleep(0.1)

if __name__ == "__main__":
    client = ChatClient()
    client.start()

