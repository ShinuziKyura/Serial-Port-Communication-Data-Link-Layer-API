public class Client extends DatagramConnection{
	public static void main(String[] args) {
		Client c = new Client(25565);
	
		c.register("11-A2-34 jose", "localhost", 25566);
		System.out.println(c.receive());
		c.register("13-B2-36 maria", "localhost", 25566);
		System.out.println(c.receive());
		c.register("13-B2-36 josefina", "localhost", 25566);
		System.out.println(c.receive());
		c.lookup("13-B2-36", "localhost", 25566);
		System.out.println(c.receive());
		c.lookup("13-B2-37", "localhost", 25566);
		System.out.println(c.receive());
		c.exit("localhost", 25566);
	}
	
	public Client(int port){
		super(port);
	}
	
	public void register(String message, String ip, int port)
	{
		this.send("REGISTER " + message, ip, port);
	}
	
	public void lookup(String message, String ip, int port)
	{
		this.send("LOOKUP " + message, ip, port);
	}
	
	public void exit(String ip, int port)
	{
		this.send("EXIT", ip, port);
	}
}
