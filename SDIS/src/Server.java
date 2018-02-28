import java.util.Hashtable;
import java.util.Timer;
import java.util.TimerTask;
import java.io.IOException;

public class Server {
	private DatagramConnection socket;
	private Hashtable<String, String> info;
	private Timer timer;
	
	public static String READER_ADDRESS = "89.152.47.157";
	
	private static final String NULL_ADDRESS = "0.0.0.0";
	private static final int NULL_PORT = 0;

	private class DatagramService extends TimerTask {
		private MulticastConnection socket;
		private String message;
		
		public static final int period = 1000;
		
		public DatagramService(int socket, String address, int port) throws IOException {
			this.service_socket = new DatagramConnection(NULL_PORT, address, port);
			this.message = "multicast "+address+":"+port+" - "+ADDRESS+":"+socket;
		}
		
		public void run() {
			try {
				this.service_socket.send(message);
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}

	public Server(int socket, String address, int port) throws IOException {
		this.reader_socket = new DatagramConnection(socket, NULL_ADDRESS, NULL_PORT);
		this.info = new Hashtable<String, String>();
		this.timer = new Timer();
		this.timer.schedule(new DatagramService(socket, address, port), 0, DatagramService.period);
	}

	public void processRequests() {
		String[] request = null;
		do {
			request = this.receive().split(" ");
		} while (this.process(request));
	}


	private boolean process(String[] request) {
		switch (request[0])
		{
		case "REGISTER":
			if(!info.containsKey(request[1])){
				info.put(request[1],request[2]);
				this.send(info.size()+"", "localhost", 25565);
				System.out.println("Registered vehicle!");
			}
			else{
				this.send(-1+"", "localhost", 25565);
				System.out.println("Unable to register vehicle...");
			}
			break;
		case "LOOKUP":
			if(info.containsKey(request[1])){
				this.send(info.get(request[1]), "localhost", 25565);
				System.out.println("Vehicle found!");
			}
			else{
				this.send("NOT_FOUND", "localhost", 25565);
				System.out.println("Vehicle not found!");
			}
			break;
		case "EXIT":
			System.out.println(request[0]);
			return false;
		}
		return true;
	}
	
	public static void main(String[] args) {
		Server s = new Server(25566);

		s.processRequests();
	}
}
