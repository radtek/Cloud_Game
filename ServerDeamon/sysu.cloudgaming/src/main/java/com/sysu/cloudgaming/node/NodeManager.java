package com.sysu.cloudgaming.node;


import java.io.File;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketAddress;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;


import org.apache.mina.core.buffer.IoBuffer;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.input.SAXBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.sysu.cloudgaming.config.Config;
import com.sysu.cloudgaming.node.network.NodeMessage;
import com.sysu.cloudgaming.node.network.NodeNetwork;
import com.sysu.cloudgaming.node.network.bean.NodeRunResponseBean;
import com.sysu.cloudgaming.node.network.upnp.GatewayDevice;
import com.sysu.cloudgaming.node.network.upnp.GatewayDiscover;
import com.sysu.cloudgaming.node.network.upnp.PortMappingEntry;
import com.sysu.cloudgaming.node.network.upnp.UPNPManager;

public class NodeManager {
	private static NodeManager manager=null;
	private static Logger logger = LoggerFactory.getLogger(NodeManager.class);
	private Map<String,ProgramBean> programMap=new HashMap<String,ProgramBean>();
	private boolean runningFlag=false;
	private ProgramBean runningbean=null;
	private int errorCode=0;
	private Process gameProcess=null;
	private Process deamonProcess=null;
	private NodeNetwork nodeNetwork=new NodeNetwork();
	private UPNPManager upnpManager=new UPNPManager();
	
	private int localPort=20000;
	public int getLocalPort() {
		return localPort;
	}
	public void setLocalPort(int localPort) {
		this.localPort = localPort;
	}


	private Random random=new Random();
	private class WatchDogThread extends Thread
	{
		public void run()
		{
			
			while(true)
			{
				try
				{
					sleep(5000);
				}
				catch(Exception e)
				{
					
				}
				try
				{
					gameProcess.exitValue();
					logger.info("Game Process Exited");
					break;
				}
				catch(IllegalThreadStateException e)
				{
					
				}
				try
				{
					deamonProcess.exitValue();
					logger.info("Daemon Process Exited");
					break;
				}
				catch(IllegalThreadStateException e)
				{
					
				}
			}
			killAllProcess();
		}
	}
	WatchDogThread watchThread=null;
	private void killAllProcess()
	{
		try
		{
			gameProcess.destroy();
			logger.info("Game Process was killed");
		}
		catch(Exception e)
		{
			logger.warn("Try to terminate game process But Failed");
		}
		try
		{
			deamonProcess.destroy();
			logger.info("Daemon Process was killed");
		}
		catch(Exception e)
		{
			logger.warn("Try to terminate deamon process But Failed");
		}
		runningFlag=false;
		//nodeNetwork.sendRunningFinishMessage(true, 0);

	}
	public static NodeManager getNodeManager()
	{
		
		if(manager==null)
		{
			manager=new NodeManager();
		}
		return manager;
	}
	private NodeManager()
	{
		
		
	}
	public ProgramBean getRunningApplicationProgramBean()
	{
		return this.runningbean;
	}
	public boolean initNodeManager()
	{
		if(!searchLocalProgram())
		{
			logger.warn("Failed to search local program!");
        	return false;
		}
		if(!nodeNetwork.setupNodeNetwork())
        {
        	logger.warn("Init Node Network Failed!");
        	return false;
        }
		if(!upnpManager.initManager())
		{
			logger.warn("Unable to find upnp device!");
		}
		return true;
	}
	public boolean isNodeRunningApplication()
	{
		return this.runningFlag;
	}
	public int getLastError()
	{
		return this.errorCode;
	}
	public NodeRunResponseBean startApplication(String programId,int quality)
	{
		NodeRunResponseBean b=new NodeRunResponseBean();
		if(runningFlag)
			return null;
		runningFlag=true;
		if(upnpManager.isInit())
		{
			logger.info("Try to open upnp port");
		
			if(upnpManager.setupUPNPMapping(localPort))//Some Error may happen!!!!
			{
				logger.info("UPNP port open ok!");
				b.setServerIp(upnpManager.getExternalIPAddress());
				b.setPort(upnpManager.getOutboundPort());
			}
			else
			{
				logger.warn("Unable to open port");
			}
		}
		if(b.getServerIp()==null)
		{
			logger.info("Simple return localaddress and port");
			b.setPort(localPort);
			try
			{
				b.setServerIp(InetAddress.getLocalHost().getHostAddress());
			}
			catch(Exception e)
			{
				logger.warn("Unable to get local node address!",e);
				killAllProcess();
				return null;
			}
		}
		if(!executeDeamonApplication(quality))
		{
			logger.warn("Unable to init server deamon");
			killAllProcess();
			return null;
		}
		if(!executeGameApplication(programId))
		{
			logger.warn("Unable to init game application");
			killAllProcess();
			return null;
		}
		
		watchThread=new WatchDogThread();
		watchThread.start();
		return b;
	}
	private boolean executeDeamonApplication(int quality)
	{
		try
		{
			ProcessBuilder builder=new ProcessBuilder(Config.DEAMONPATH,"-q "+quality);
			gameProcess=builder.start();
			return true;
		}
		catch(Exception e)
		{
			logger.warn(e.getMessage(),e);
			return false;
		}
		
	}
	private boolean executeGameApplication(String programId)
	{
		if(!programMap.containsKey(programId))
		{
			logger.warn("Local disk don't have such game!");
			return false;
		}
		else
		{
			this.runningbean=programMap.get(programId);
			logger.info("Try to execute {} in {}",runningbean.getProgramName(),runningbean.getProgramPath());
			try
			{
				ProcessBuilder builder=new ProcessBuilder(runningbean.getProgramPath());
				deamonProcess=builder.start();
				return true;
			}
			catch(Exception e)
			{
				logger.warn(e.getMessage(),e);
				return false;
			}
		}
	}
	
	public boolean shutdownApplication()
	{
		if(!runningFlag)
		{
			logger.warn("Application is not running yet");
			return false;
		}
		else
		{
			killAllProcess();
			return true;
		}
	}
	
	public boolean sendRunningFinishMessage(boolean successful,int errorcode)
	{
		NodeMessage msg=new NodeMessage();
		msg.setMessageType(NodeMessage.RUNNINGFINISHMESSAGE);
		msg.setSuccess(successful);
		msg.setErrorCode(errorcode);
		nodeNetwork.getNetworkSession().write(msg);
		return true;
	}
	
	
	/*
	 * UDP HOLE FUNCTION
	 */
/*	private SocketAddress requestStunForPublicAddress()
	{
		DatagramSocket client=null;
		byte[] sendBuf=new byte[32];
		byte[] recvBuf = new byte[32];
		DatagramPacket sendPacket =null;
		DatagramPacket recvPacket=null;
		try
		{
			client = new DatagramSocket();
			client.setReuseAddress(true);
			client.setSoTimeout(3000);
			
			sendPacket  = new DatagramPacket(sendBuf ,sendBuf.length , InetAddress.getByName(Config.HUBSERVERADDR) , Config.STUNPORT);
			recvPacket= new DatagramPacket(recvBuf , recvBuf.length);
		}
		catch(Exception e)
		{
			logger.warn("Init Socket client error");
		}
		int tryTime=0;
		while(tryTime<8)
		{
			try
			{
				client.send(sendPacket);
				client.receive(recvPacket);
				IoBuffer buffer=IoBuffer.allocate(recvBuf.length);
				buffer.put(recvBuf);
				buffer.flip();
				byte []ipaddress=new byte[4];
				buffer.get(ipaddress);
				int port=buffer.getInt();
				break;
			}
			catch(Exception e)
			{
				logger.warn("Recv Packet Failed! Retrying",e);
				tryTime++;
			}
		}
    	return null;
	}*/
	
	public  boolean searchLocalProgram()
	{
		
		try
		{
			programMap.clear();
			File infoFile=new File(Config.LOCALPROGRAMPATH+"/"+Config.LOCALPROGRAMXMLNAME);
			if(infoFile.exists())
			{
				SAXBuilder builder=new SAXBuilder();
				
					Document doc=builder.build(infoFile);
					Element info=doc.getRootElement();
					List<Element> games=info.getChildren("program");
					logger.info("Local disk have {} game",games.size());
					for(Element g: games)
					{
						ProgramBean b=new ProgramBean();
						b.setProgramID(g.getChildText("id"));
						b.setProgramName(g.getChildText("name"));
						b.setProgramVersion(g.getChildText("ver"));
						b.setProgramPath(Config.LOCALPROGRAMPATH+'/'+g.getChildText("path"));
						logger.info("Add Game to Map Id:{}, Name:{}",b.getProgramID(),b.getProgramName());
						programMap.put(b.getProgramID(), b);
					}
					return true;
				
				
			}
			else
			{
				logger.warn("Info File Not Existed!");
			}
			return false;
		}
		
		catch(Exception e)
		{
			logger.warn(e.getMessage(),e);
			return false;
		}
	}
	
}
