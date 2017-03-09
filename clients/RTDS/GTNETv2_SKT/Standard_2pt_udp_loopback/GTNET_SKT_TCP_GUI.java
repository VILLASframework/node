import java.awt.Color;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.util.Vector;

import javax.swing.BorderFactory;
import javax.swing.DefaultCellEditor;
import javax.swing.JComboBox;
import javax.swing.JFileChooser;
import javax.swing.JFormattedTextField;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.ListSelectionModel;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;
import javax.swing.border.CompoundBorder;
import javax.swing.border.EmptyBorder;
import javax.swing.border.TitledBorder;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableCellEditor;

/*
 * NOTE: GTNET_SKT_TCP_GUI was made as an example TCP client/server
 * 		 The code is given as an example and is not intended to be 
 * 		 part of RSCAD support. Questions will not be answered.
 */

public class GTNET_SKT_TCP_GUI extends JFrame
{
    /**
	 * 
	 */
	private static final long serialVersionUID = -6325275282238226823L;

	private static boolean DEBUG = false;

    static TCP_ListenTask serverProcess;
    static TCP_ConnectTask clientProcess;
	//UDP Server
	static JTextField jTextFieldLocalIp;
	static JTextField jTextFieldLocalPort;
	static JLabel lblLocalIp;
	static JLabel lblLocalPort;
	public static SoftEdgeButton btnStart;
	public static SoftEdgeButton btnTCP;
	public static SoftEdgeButton btnQuit;
	public static SoftEdgeButton btnAddIp;
	public static SoftEdgeButton btnDeleteIp;
	public static SoftEdgeButton btnSaveIp;
	public static SoftEdgeButton btnLoadIp;
	static JTable IPtable;
	//UDP Client
	static JTextField jTextFieldRemoteIp;
	static JTextField jTextFieldRemotePort;
	static JLabel lblRemoteIp;
	static JLabel lblRemotePort;

	public static SoftEdgeButton btnSend;
	static JTable OPtable;
	private static JPanel MainPnl;
	
	final static int GTNETSKT_MAX_INPUTS = 300;
	
	public static String[] gtnetSktInputName = new String[GTNETSKT_MAX_INPUTS];
	public static String[] gtnetSktInputType = new String[GTNETSKT_MAX_INPUTS];
	static int ival[];
	static float fval[];
	public static String[] gtnetSktOutputName = new String[GTNETSKT_MAX_INPUTS];
	public static String[] gtnetSktOutputType = new String[GTNETSKT_MAX_INPUTS];

	static int numInPoints = 0;
	static int numOutPoints = 0;
    static InetAddress host = null;
    static byte[] inBuf = null;
	static byte[] outBuf = null;
    static String msg = null;
    public static TableCellEditor editorInt;
    public static TableCellEditor editorFloat;
    protected static JTextArea textArea;
 //   private final static String newline = "\r\n";
    private final static String newline = System.getProperty("line.separator");//This will retrieve line separator 
    private final static String delim = "\t";//","; // tab delimited file
    
	ServerSocket serverSocket = null;
	Socket connectionSocket =null;
	static GTNET_SKT_TCP_GUI main_inst;

	public static void main(String args[])
	{
	    SwingUtilities.invokeLater(new Runnable() {
	        public void run() {
	        	main_inst = new GTNET_SKT_TCP_GUI();
	        	main_inst.setVisible(true);
	        }
	    });
		
	}
	
    public GTNET_SKT_TCP_GUI()
    {
        super("RTDS GTNET-SKT TCP Server and Client");        
        
		try
		{
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		}
		catch(Exception e)
		{
			
		}
		
		JFormattedTextField ftf[] = new JFormattedTextField[2];
		ftf[0] = new JFormattedTextField(new Integer(90032221));
		ftf[1] = new JFormattedTextField(java.text.NumberFormat.getInstance());
		ftf[1].setValue(new Float(0.0));
		editorInt = new DefaultCellEditor(ftf[0]);
		editorFloat = new DefaultCellEditor(ftf[1]);
		
		Object options[] = { "int", "float" };
		JComboBox<?> comboBox = new JComboBox<Object>(options);
		comboBox.setMaximumRowCount(4);
		TableCellEditor editorType = new DefaultCellEditor(comboBox);

		// The Server Table
		String columnNames[] = {"Point #","Variable","Type","Value"};
		IPtable = new JTable(new InputTableModel(columnNames));//Create JTable
		IPtable.setCellSelectionEnabled(true);	 
		IPtable.setPreferredScrollableViewportSize(new Dimension(200, 150));
		IPtable.setFillsViewportHeight(true);
		IPtable.getTableHeader().setReorderingAllowed(false); //no column dragging        
		IPtable.setRowSelectionAllowed(true);
		IPtable.setColumnSelectionAllowed(false);
		IPtable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        JTableHeader header = IPtable.getTableHeader();
        header.setBackground(Color.LIGHT_GRAY);
        IPtable.getColumnModel().getColumn(2).setCellEditor(editorType);
		
    	// The Client Table
		OPtable = new JTable(new OutputTableModel(columnNames));//Create JTable
		OPtable.setPreferredScrollableViewportSize(new Dimension(200, 150));
		OPtable.setFillsViewportHeight(true);
		OPtable.getTableHeader().setReorderingAllowed(false); //no column dragging  
		OPtable.setRowSelectionAllowed(true);
		OPtable.setColumnSelectionAllowed(false);
		OPtable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        OPtable.getTableHeader().setBackground(Color.LIGHT_GRAY);
        OPtable.getColumnModel().getColumn(2).setCellEditor(editorType);

    	setup(); // The GUI

		setSize(520,745);
		setLayout(new FlowLayout());
		setDefaultCloseOperation(EXIT_ON_CLOSE);
        this.getContentPane().add(MainPnl);     
		setVisible(true);

    }
    
    public static void setup()
    {
		try {
			host = InetAddress.getLocalHost();
		} catch (UnknownHostException e) {
			e.printStackTrace();
		}    		
 
		// Main Panel
		MainPnl	= new JPanel(new GridBagLayout());			
		GridBagConstraints gbc = new GridBagConstraints();		
		gbc.gridx = 0;
		gbc.gridy = 0;
		gbc.gridwidth = 1;
		MainPnl.add(createTCP_Pnl(), gbc);
		gbc.gridx = 0;
		gbc.gridy = 1;
		gbc.gridwidth = 1;
		MainPnl.add(createInputPointPnl(), gbc);
		gbc.gridx = 0;
		gbc.gridy = 2;
		gbc.gridwidth = 1;
		MainPnl.add(createOutputPointPnl(), gbc);
		gbc.gridx = 0;
		gbc.gridy = 3;
		gbc.gridwidth = 1;
		MainPnl.add(createMsgPnl(), gbc);
			  	
    }

	private static JPanel createTCP_Pnl() {
		//Points from GTNET-SKT
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "TCP Communication Mode");
		JPanel pnlTCP = new JPanel(new GridBagLayout());
		pnlTCP.setBorder(new CompoundBorder(borderPoints, new EmptyBorder(2,2,2,2)));
		Object options[] = { "Client", "Server" };
		final JComboBox<?> comboBox = new JComboBox<Object>(options);
		btnTCP= new SoftEdgeButton("Connect");

		lblLocalIp = new JLabel("Local Server, IP Address:");
		jTextFieldLocalIp = new JTextField("", 10);
		jTextFieldLocalIp.setText(host.getHostAddress());
		jTextFieldLocalIp.setEditable(true);
		
		lblLocalPort = new JLabel("Port");
		jTextFieldLocalPort = new JTextField("", 6);
		jTextFieldLocalPort.setText("12345");

    	jTextFieldLocalIp.setVisible(false);
    	jTextFieldLocalPort.setVisible(false);
    	lblLocalIp.setVisible(false);
    	lblLocalPort.setVisible(false);
		
		lblRemoteIp = new JLabel("Remote Server, IP Address:");
		jTextFieldRemoteIp = new JTextField("", 9);
		jTextFieldRemoteIp.setText(host.getHostAddress());
        if (DEBUG) {
    		jTextFieldRemoteIp.setText("172.24.9.185");
        }
		jTextFieldRemoteIp.setEditable(true);
		
		lblRemotePort = new JLabel("Port");
		jTextFieldRemotePort = new JTextField("", 5);
        jTextFieldRemotePort.setText("12345");

		
		comboBox.addActionListener(new ActionListener(){
			@Override
			public void actionPerformed(ActionEvent e) {
			    if(comboBox.getSelectedIndex()==0)
			    {
			    	btnTCP.setText("Connect");
			    	jTextFieldLocalIp.setVisible(false);
			    	jTextFieldLocalPort.setVisible(false);
			    	lblLocalIp.setVisible(false);
			    	lblLocalPort.setVisible(false);
					lblRemoteIp.setVisible(true);
					jTextFieldRemoteIp.setVisible(true);
					lblRemotePort.setVisible(true);
					jTextFieldRemotePort.setVisible(true);
			    }
			    else
			    {
			    	btnTCP.setText("Listen");				
			    	jTextFieldLocalIp.setVisible(true);
			    	jTextFieldLocalPort.setVisible(true);
			    	lblLocalIp.setVisible(true);
			    	lblLocalPort.setVisible(true);
					lblRemoteIp.setVisible(false);
					jTextFieldRemoteIp.setVisible(false);
					lblRemotePort.setVisible(false);
					jTextFieldRemotePort.setVisible(false);										
			    }
			
			}
			
		});  
		
		btnTCP.addActionListener(new ActionListener() {
			@SuppressWarnings("deprecation")
			@Override
			public void actionPerformed(ActionEvent e) {
			    if(btnTCP.getText()=="Listen")
			    {
			    	echo("Server socket opening...");			        
			        try {
			        	serverProcess = new TCP_ListenTask(main_inst , numInPoints);
			        	serverProcess.start();
			        } 
			        catch (Exception e1) 
			        {
			        	echo(e1.getMessage());
			        }

			    	return;
			    }
			    if(btnTCP.getText()=="StopServer")
			    {
			    	btnTCP.setText("Listen");
			        btnAddIp.setEnabled(true);
			        btnDeleteIp.setEnabled(true);
			        btnSaveIp.setEnabled(true);
			        btnLoadIp.setEnabled(true);
			    	jTextFieldLocalIp.setEnabled(true);
			    	jTextFieldLocalPort.setEnabled(true);
			    	btnSend.setEnabled(false);
			    	echo("Server socket closed...");			        
			    	serverProcess.closeTask();
			    	serverProcess.stop();
			    	return;
			    }
			    if(btnTCP.getText()=="Connect")
			    {
					echo("Client socket opening...");
			        try {
			        	clientProcess = new TCP_ConnectTask(main_inst , numOutPoints);
			        	clientProcess.start();
			        } 
			        catch (Exception e1) 
			        {
			        	echo(e1.getMessage());
			        }
			    	return;
			    }				
			    if(btnTCP.getText()=="StopClient")
			    {
			    	clientProcess.closeTask();
			    	echo("Client socket closed...");			        
			    	return;
			    }				
			}			
		});
		
		GridBagConstraints gbc = new GridBagConstraints();		
		JPanel TCPoptions = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
	    TCPoptions.add(comboBox, gbc);
	    TCPoptions.add(btnTCP, gbc);
		
		JPanel LocalPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
		LocalPanel.add(comboBox, gbc);
		LocalPanel.add(btnTCP, gbc);
		LocalPanel.add(lblLocalIp, gbc);
		LocalPanel.add(jTextFieldLocalIp, gbc);
		LocalPanel.add(lblLocalPort, gbc);
		LocalPanel.add(jTextFieldLocalPort, gbc);

		JPanel RemotePanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
		
	    RemotePanel.add(lblRemoteIp, gbc);
	    RemotePanel.add(jTextFieldRemoteIp, gbc);
	    RemotePanel.add(lblRemotePort, gbc);
	    RemotePanel.add(jTextFieldRemotePort, gbc);
		
	    gbc = new GridBagConstraints();	
	    gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 1;
		gbc.gridy = 0;
		pnlTCP.add(TCPoptions, gbc);
		gbc.gridx = 2;
		gbc.gridy = 0;
		pnlTCP.add(LocalPanel, gbc);
		gbc.gridx = 3;
		gbc.gridy = 0;
		pnlTCP.add(RemotePanel, gbc);
		return pnlTCP;

	}
	
	private static JPanel createInputPointPnl() {
		
		//Points from GTNET-SKT
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "TCP Server");
		JPanel pnlPoint = new JPanel(new GridBagLayout());
		pnlPoint.setBorder(new CompoundBorder(borderPoints, new EmptyBorder(5,10,5,10)));
		      
		GridBagConstraints gbc = new GridBagConstraints();		

		//Create the scroll pane and add the table to it.
        JScrollPane scrollPane = new JScrollPane(IPtable);
		gbc.gridx = 0;
		gbc.gridy = 0;
		gbc.anchor = GridBagConstraints.WEST;
	    gbc.weightx = 1;
	    gbc.fill = GridBagConstraints.BOTH;
	    pnlPoint.add(scrollPane,gbc);
		
		btnStart= new SoftEdgeButton("Start");
		btnAddIp = new SoftEdgeButton("Add") ;
		btnDeleteIp = new SoftEdgeButton("Delete") ;
	    btnSaveIp = new SoftEdgeButton("Save Table");   
	    btnLoadIp = new SoftEdgeButton("Load Table");   

		btnAddIp.addActionListener(new ActionListener() {
			@Override
		     public void actionPerformed(ActionEvent e) 
		     {
				if(numInPoints< GTNETSKT_MAX_INPUTS){
					int i = numInPoints++;
					InputTableModel a = (InputTableModel) IPtable.getModel();
					gtnetSktInputName[i] = "POINTin" + Integer.toString(i);
					gtnetSktInputType[i] = "float";
					Object[] values = {i, gtnetSktInputName[i], gtnetSktInputType[i], ""};
					a.insertData(values);
				}else{
					echo("Number of Server Points is at maximum of:" + GTNETSKT_MAX_INPUTS);
				}				
		     }			
		});
		
		btnDeleteIp.addActionListener(new ActionListener(){
			@Override
			public void actionPerformed(ActionEvent e) {
		    	 if(IPtable.getSelectedRow() != -1){
		    		 InputTableModel a = (InputTableModel) IPtable.getModel();
		    		 a.removeRow(IPtable.getSelectedRow());
		    		 numInPoints--;
		    		 for (int i=0; i < numInPoints; i++) {
		    			 a.setValueAt((Object) i, i, 0);
		    		 }
		    	 }
			}
		});
		
        btnSaveIp.addActionListener(new ActionListener() {   
			@Override
            public void actionPerformed(ActionEvent e) {   
                saveIPTable();   
            }   
        });   

        btnLoadIp.addActionListener(new ActionListener() {   
			@Override
            public void actionPerformed(ActionEvent e) {   
                loadIPTable();   
            }   
        });   
		
		JPanel tableButtonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
	    tableButtonPanel.add(btnAddIp, gbc);
	    tableButtonPanel.add(btnDeleteIp, gbc);
	    tableButtonPanel.add(btnSaveIp, gbc);
	    tableButtonPanel.add(btnLoadIp, gbc);

		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 1;
		pnlPoint.add(tableButtonPanel, gbc);

		return pnlPoint;
		
	}
	
	private static JPanel createOutputPointPnl() {
		
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "TCP Client");

		//Points to GTNET-SKT
		JPanel pnlPoint = new JPanel(new GridBagLayout());

		pnlPoint.setBorder(new CompoundBorder(borderPoints, new EmptyBorder(5,10,5,10)));
              
		GridBagConstraints gbc = new GridBagConstraints();		

		//Create the scroll pane and add the table to it.
        JScrollPane scrollPane = new JScrollPane(OPtable);
		gbc.gridx = 0;
		gbc.gridy = 0;
		gbc.anchor = GridBagConstraints.WEST;
	    gbc.weightx = 1;
	    gbc.fill = GridBagConstraints.BOTH;
	    pnlPoint.add(scrollPane,gbc);
		
		btnSend= new SoftEdgeButton("Send");
		btnSend.setEnabled(false);
		SoftEdgeButton btnAddOp = new SoftEdgeButton("Add") ;
		SoftEdgeButton btnDeleteOp = new SoftEdgeButton("Delete") ;
	    SoftEdgeButton btnSaveOp = new SoftEdgeButton("Save Table");   
	    SoftEdgeButton btnLoadOp = new SoftEdgeButton("Load Table");   

		btnSend.addActionListener(new ActionListener() {  
	        int txCnt = 0;
			@Override
			public void actionPerformed(ActionEvent e) {

				int size = numOutPoints * 4;
				  
				try {
				    byte[] buffer = new byte[size];
				    ByteBuffer outbuff =  ByteBuffer.wrap(buffer);
				    txCnt++;
		      		for (int i = 0; i<numOutPoints;i++)
		      		{
		      			if(gtnetSktOutputType[i].equalsIgnoreCase("int"))
		      			{	    
		      				//byte[] src = GTNET_SKT_UDP.convertToByteArray(Integer.valueOf((String) GTNET_SKT_UDP.OPtable.getValueAt(i, 3))); 
		      				// Note: if input is sent as float we can just round and the return will always be an INT so no exception will be thrown as with above line of code
			      			byte[] src = convertToByteArray(Math.round(Float.valueOf((String) OPtable.getValueAt(i, 3))));  
			      			outbuff.put(src, 0, 4);
		      			}
		      			if(gtnetSktOutputType[i].equalsIgnoreCase("float"))
		      			{
			      			byte[] src = convertToByteArray(Float.valueOf((String) OPtable.getValueAt(i, 3))); 
			      			outbuff.put(src, 0, 4);
		      			}
		      		}
		      		buffer = outbuff.array();
				    
					DataOutputStream outToClient = new DataOutputStream(main_inst.connectionSocket.getOutputStream());
					outToClient.write(buffer, 0, size);
					
					echo(main_inst.connectionSocket.getRemoteSocketAddress()+" - Packets Sent ="+String.valueOf(txCnt));
					
				} catch (UnknownHostException ex) {
					ex.printStackTrace();
				} catch (SocketException ex) {
					ex.printStackTrace();
				} catch (IOException ex) {
					ex.printStackTrace();
				}
				catch(Exception ex) {
					System.out.print("Client Error! It didn't work!\n" + ex.getMessage());
				}        
			}
        });
		
		btnAddOp.addActionListener(new ActionListener() {
			@Override
		    public void actionPerformed(ActionEvent e) 
		    {
				if(numOutPoints< GTNETSKT_MAX_INPUTS){
					int i = numOutPoints++;
					OutputTableModel a = (OutputTableModel) OPtable.getModel();
					gtnetSktOutputName[i] = "POINT" + Integer.toString(i);
					gtnetSktOutputType[i] = "float";
					Object[] values = {i, gtnetSktOutputName[i], gtnetSktOutputType[i], Float.toString(i)};
					a.insertData(values);
				}else{
					echo("Number of Client Points is at maximum of:" + GTNETSKT_MAX_INPUTS);
				}
		    }			
		});
		
		btnDeleteOp.addActionListener(new ActionListener() {
			@Override
		    public void actionPerformed(ActionEvent e) 
		    {
		    	 if(OPtable.getSelectedRow() != -1){
		    		 OutputTableModel a = (OutputTableModel) OPtable.getModel();
		    		 a.removeRow(OPtable.getSelectedRow());
		    		 numOutPoints--;
		    		 for (int i=0; i < numOutPoints; i++) {
		    			 a.setValueAt((Object) i, i, 0);
		    		 }
		    	 }
		    }			
		});
		
		btnSaveOp.addActionListener(new ActionListener() {   
            public void actionPerformed(ActionEvent e) {   
                saveOPTable();   
            }   
        });   

		btnLoadOp.addActionListener(new ActionListener() {   
            public void actionPerformed(ActionEvent e) {   
                loadOPTable();   
            }   
        });   
		
		JPanel tableButtonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
		tableButtonPanel.add(btnAddOp, gbc);
		tableButtonPanel.add(btnDeleteOp, gbc);
		tableButtonPanel.add(btnSaveOp, gbc);
		tableButtonPanel.add(btnLoadOp, gbc);
		tableButtonPanel.add(btnSend, gbc);

		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 1;
//		pnlPoint.add(remotePanel, gbc);
//		gbc.gridx = 0;
//		gbc.gridy = 2;
		pnlPoint.add(tableButtonPanel, gbc);

		return pnlPoint;
		
	}
    
	private static JPanel createMsgPnl() {
		
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "Messages");

		//Points to GTNET-SKT
		JPanel pnlMsg = new JPanel(new GridBagLayout());

		pnlMsg.setBorder(new CompoundBorder(borderPoints, new EmptyBorder(5,10,5,10)));
              
		GridBagConstraints gbc = new GridBagConstraints();		

		//Create the scroll pane and add the text area to it.
        textArea = new JTextArea(7, 40);
        textArea.setEditable(false);
        textArea.setAutoscrolls(true);
        Font font = new Font("Verdana", Font.LAYOUT_LEFT_TO_RIGHT, 10);
        textArea.setFont(font);

        JScrollPane scrollPane = new JScrollPane(textArea);
		gbc.gridx = 0;
		gbc.gridy = 0;
		gbc.anchor = GridBagConstraints.WEST;
	    gbc.weightx = 1;
	    gbc.fill = GridBagConstraints.BOTH;
	    pnlMsg.add(scrollPane,gbc);
		
		return pnlMsg;
		
	}

	static JFileChooser myJFileChooser = new JFileChooser(new File("."));
	
    private static void saveOPTable() {   
		if (fileChooser(false,"*.txt") !=  null) {   
            saveOPTable(myJFileChooser.getSelectedFile().toString());   
        }   
    }   
    private static void saveOPTable(String myfile) {   
        try {   
        	StringBuffer fileContent = new StringBuffer();
        	OutputTableModel tModel = (OutputTableModel) OPtable.getModel();
        	for (int i = 0; i < tModel.getRowCount(); i++) {

        	     Object cellValue = tModel.getValueAt(i, 1) + delim + tModel.getValueAt(i, 2) + newline;
        	     // ... continue to read each cell in a row
        	     fileContent.append(cellValue);
        	     // ... continue to append each cell value
        	}
        	FileWriter fileWriter = new FileWriter(new File(myfile));
        	fileWriter.write(fileContent.toString());
        	fileWriter.flush();
        	fileWriter.close();            }   
            catch (Exception ex) {   
                ex.printStackTrace();   
            }   
    }   
 
    private static void saveIPTable() {  
		if (fileChooser(false,"*.txt") !=  null) {   
			saveIPTable(myJFileChooser.getSelectedFile().toString());   
		}   
    }   
    private static void saveIPTable(String myfile) {   
        try {   
        	StringBuffer fileContent = new StringBuffer();
        	InputTableModel tModel = (InputTableModel) IPtable.getModel();
        	for (int i = 0; i < tModel.getRowCount(); i++) {

        	     Object cellValue = tModel.getValueAt(i, 1) + delim + tModel.getValueAt(i, 2) + newline ;
        	     // ... continue to read each cell in a row
        	     fileContent.append(cellValue);
        	     // ... continue to append each cell value
        	}
        	FileWriter fileWriter = new FileWriter(new File(myfile));
        	fileWriter.write(fileContent.toString());
        	fileWriter.flush();
        	fileWriter.close();            }   
            catch (Exception ex) {   
                ex.printStackTrace();   
            }   
    }   
   
    private static void loadOPTable() {   
        int i = 0;
    	if (DEBUG) {
            i = InitParamsFromFile("C:\\RTDS_USER\\fileman\\Canada\\Deans\\Devel\\GTNET-SKT\\GTNETSKT1_out.txt",gtnetSktOutputName,gtnetSktOutputType);
        }
        else
        {
            i = InitParamsFromFile("*.txt",gtnetSktOutputName,gtnetSktOutputType);
        }
         if(i!=0){
        	numOutPoints = i;
        }
		OutputTableModel a = (OutputTableModel) OPtable.getModel();
        int x = OPtable.getRowCount();
        for (i=0; i < x ; i++) {
        	a.removeRow(x-i-1);
        }      	
        fillOutputData(OPtable, numOutPoints, gtnetSktOutputName, gtnetSktOutputType);
    }   

    private static void loadIPTable() {   
        int i = 0;
    	if (DEBUG) {
        	i = InitParamsFromFile("C:\\RTDS_USER\\fileman\\Canada\\Deans\\Devel\\GTNET-SKT\\GTNETSKT1_to.txt",gtnetSktInputName,gtnetSktInputType);
        }
        else
        {
        	i = InitParamsFromFile("*.txt",gtnetSktInputName,gtnetSktInputType);
        }
        if(i!=0){
        	numInPoints = i;
        }
		ival = new int[numInPoints];
		fval = new float[numInPoints];
		InputTableModel a = (InputTableModel) IPtable.getModel();
        int x = IPtable.getRowCount();
        for (i=0; i < x ; i++) {
        	a.removeRow(x-i-1);
        }      	
        fillInputData(IPtable, numInPoints, gtnetSktInputName, gtnetSktInputType);
    }   

    class InputTableModel extends AbstractTableModel {
		
		/**
		 * 
		 */
		private static final long serialVersionUID = -2870203912612672882L;
		private String[] columnNames;
		private Vector<Vector<?>> data = new Vector<Vector<?>>();
 
    	public InputTableModel(String[] cNames)
    	{
    		columnNames = cNames;
    	}
 
		@Override
		public int getColumnCount() {
			return columnNames.length;
		}
 
		@Override
		public int getRowCount() {
			return data.size();
		}
 
		@Override
		public Object getValueAt(int row, int col) {
			return ((Vector<?>) data.get(row)).get(col);
		}
 
		public String getColumnName(int col){
			return columnNames[col];
		}
		
		public Class<? extends Object> getColumnClass(int c){
			return getValueAt(0,c).getClass();
		}
 
		@SuppressWarnings("unchecked")
		public void setValueAt(Object value, int row, int col){
            if (DEBUG) {
                System.out.println("Setting value at " + row + "," + col
                                   + " to " + value
                                   + " (an instance of "
                                   + value.getClass() + ")");
            }
            if (value instanceof Object && col == 0) {
    			((Vector<Object>) data.get(row)).setElementAt(value, col);
     			fireTableCellUpdated(row,col);
            }

            if (value instanceof String && col == 1) {
    			String newValue = (String) value;    			   			
    			// check for valid length
 	        	if (newValue.length()<=10) {
	    			((Vector<String>) data.get(row)).setElementAt(newValue, col);
	    			fireTableCellUpdated(row,col);
	                if (DEBUG) {
	                    System.out.println("New value of data:");
	                    printDebugData();
	                }
	        	}
				else
				{
					JOptionPane.showMessageDialog(null, "Value at " + row + "," + col +  " Must be 10 characters or less", "Input Error", JOptionPane.ERROR_MESSAGE);
					return;
				}
	        }

            if (value instanceof String && col == 2) {
    			((Vector<Object>) data.get(row)).setElementAt(value, col);
    			gtnetSktInputType[row]=(String) value;
    			fireTableCellUpdated(row,col);
            }
            
            if (value instanceof String && col == 3) {
		        boolean save = false;
    			int numdecimals = 0;
//	   			int offset = 0;
    			String tempStr = (String) ((Vector<?>) data.get(row)).elementAt(2);
    			String newValue = (String) value;    			
    			newValue = newValue.replaceAll(",", "");
    			
    			// check for valid digits, decimal places, and range check
 	        	for (int i=0; i < newValue.length(); i++) {
		        	char c = newValue.charAt(i);
//		        	if(c == '-' && i == 0){
//		        		offset = 1;
//		        	}
	        		if(i > 0 && c == '.' && tempStr.equalsIgnoreCase("float")){
						numdecimals ++;
	        		}
					if ( (c == '-' && i == 0) | (c >= '0' && c <= '9') | (i > 0 && c == '.' && tempStr.equalsIgnoreCase("float")) && numdecimals<=1)
					{
						save = true;
					}
	        	}
	        	if (save)
	        	{
	    			((Vector<String>) data.get(row)).setElementAt(newValue, col);
	    			fireTableCellUpdated(row,col);
	                if (DEBUG) {
	                    System.out.println("New value of data:");
	                    printDebugData();
	                }
	        	}
				else
				{
					if(tempStr.equalsIgnoreCase("int"))
					{
						JOptionPane.showMessageDialog(null, "Value at " + row + "," + col + " Must be a 32 bit integer number from -2,147,483,648 to +2,147,483,647", "Input Error", JOptionPane.ERROR_MESSAGE);
					}
					else
					{
						JOptionPane.showMessageDialog(null, "Value at " + row + "," + col + " Must be a 32 bit float number from -2,147,483,648.0 to +2,147,483,647.0", "Input Error", JOptionPane.ERROR_MESSAGE);
					}
						
					return;
				}
	        }
		}
 
		public boolean isCellEditable(int row, int col){
            if (col != 2) {
                return false;
            }else if(btnStart.getText()=="Stop"){
                return false;            	
            } else {
                return true;
            }
		}
 
		@SuppressWarnings("unchecked")
		public void insertData(Object[] values){
		 	data.add(new Vector<Object>());
		 	for(int i =0; i<values.length; i++){
			 ((Vector<Object>) data.get(data.size()-1)).add(values[i]);
		 	}
		 	fireTableDataChanged();
	 	}
	 
	 	public void removeRow(int row){
	 		data.removeElementAt(row);
	 		fireTableDataChanged();
	 	}
 
        private void printDebugData() {
            int numRows = getRowCount();
            int numCols = getColumnCount();

            for (int i=0; i < numRows; i++) {
                System.out.print("    row " + i + ":");
                for (int j=0; j < numCols; j++) {
                    System.out.print("  " + ((Vector<?>) data.get(i)).elementAt(j) );
                }
                System.out.println();
            }
            System.out.println("----------------------------------------------------");
        }
	} // end of tableModel
    
	class OutputTableModel extends AbstractTableModel {
		
		/**
		 * 
		 */
		private static final long serialVersionUID = 8248501194260208975L;
		private String[] columnNames;
		private Vector<Vector<?>> data = new Vector<Vector<?>>();
 
    	public OutputTableModel(String[] cNames)
    	{
    		columnNames = cNames;
    	}
 
		@Override
		public int getColumnCount() {
			return columnNames.length;
		}
 
		@Override
		public int getRowCount() {
			return data.size();
		}
 
		@Override
		public Object getValueAt(int row, int col) {
			return ((Vector<?>) data.get(row)).get(col);
		}
 
		public String getColumnName(int col){
			return columnNames[col];
		}
		
		public Class<? extends Object> getColumnClass(int c){
			return getValueAt(0,c).getClass();
		}
 
		@SuppressWarnings("unchecked")
		public void setValueAt(Object value, int row, int col){
            if (DEBUG) {
                System.out.println("Setting value at " + row + "," + col
                                   + " to " + value
                                   + " (an instance of "
                                   + value.getClass() + ")");
            }
            
            if (value instanceof Object && col == 0) {
    			((Vector<Object>) data.get(row)).setElementAt(value, col);
     			fireTableCellUpdated(row,col);
            }

            if (value instanceof String && col == 1) {
    			String newValue = (String) value;    
    			// check for valid length
 	        	if (newValue.length()<=10) {
	    			((Vector<String>) data.get(row)).setElementAt(newValue, col);
	    			fireTableCellUpdated(row,col);
	                if (DEBUG) {
	                    System.out.println("New value of data:");
	                    printDebugData();
	                }
	        	}
				else
				{
					JOptionPane.showMessageDialog(null, "Must be 10 characters or less", "Input Error", JOptionPane.ERROR_MESSAGE);
					return;
				}
	        }

            if (value instanceof String && col == 2) {
    			((Vector<Object>) data.get(row)).setElementAt(value, col);
    			gtnetSktOutputType[row]=(String) value;
    			fireTableCellUpdated(row,col);
            }
            
            if (value instanceof String && col == 3) {
		        boolean save = false;
				boolean realok = true;
		        boolean fracok = true;
    			int numdecimals = 0;
	   			int offset = 0;
    			String typeStr = (String) ((Vector<?>) data.get(row)).elementAt(2);
    			String newValue = (String) value;
    			String real = null, frac = null;
     			newValue = newValue.replaceAll(",", "");
 				if(newValue.contains("-")){ 
					offset = 1;
				}
				if(typeStr.equalsIgnoreCase("float") && newValue.contains(".")){	// handles the decimal
					real = newValue.substring(offset, newValue.indexOf("."));
					frac = newValue.substring(newValue.indexOf(".")+1, newValue.length());
					if(real.length()>=10){
						for (int i=0; i < 10; i++) {
							char c = real.charAt(i);
			        		if (c > '2' && i == 0)
			        			realok = false;
			        		if (c > '1' && i == 1)
			        			realok = false;
			        		if (c > '4' && i == 2)
			        			realok = false;
			        		if (c > '7' && i == 3)
			        			realok = false;
			        		if (c > '4' && i == 4)
			        			realok = false;
			        		if (c > '8' && i == 5)
			        			realok = false;
			        		if (c > '3' && i == 6)
			        			realok = false;
			        		if (c > '6' && i == 7)
			        			realok = false;
			        		if (c > '4' && i == 8)
			        			realok = false;
			        		if (c > '7' && i == 9 && offset==0)
			        			realok = false;
			        		if (c == '7' && i == 9 && offset==0 && realok){
			        			if(real.contains("2147483647")){
									for (int j=0; j < frac.length(); j++) {
										char c1 = frac.charAt(j);
										if(c1>'0'){
											fracok = false;
											echo("This is larger than a signed 16 bit float: " + real + "." + frac);
										}
									}			        				
			        			}
			        		}
			        		if (c > '8' && i == 9 && offset==1)	// for negative numbers
			        			realok = false;
			        		if (c == '8' && i == 9 && offset==1 && realok){
			        			if(real.contains("2147483648")){
									for (int j=0; j < frac.length(); j++) {
										char c1 = frac.charAt(j);
										if(c1>'0'){
											fracok = false;
											echo("This is larger than a signed 16 bit float: " + real + "." + frac);
										}
									}			        				
			        			}
			        		}
							
						}
					}
				}
				
				if(typeStr.equalsIgnoreCase("int") | typeStr.equalsIgnoreCase("float") && !newValue.contains(".")){
					real = newValue.substring(offset, newValue.length());
				}
				if(typeStr.equalsIgnoreCase("int") && newValue.contains(".")){
					real = newValue.substring(offset, newValue.indexOf("."));
					int i = Math.round(Float.valueOf(newValue));
					newValue = Integer.toString(i);
				}
				if(real.length()<=10){
	    			// check for valid digits and maximum range check 
	 	        	for (int i=0; i < real.length(); i++) {
			        	char c = real.charAt(i);
		        		if(i > 0 && c == '.' && typeStr.equalsIgnoreCase("float")){
							numdecimals ++;
		        		}
						if ( (c == '-' && i == 0) | (c >= '0' && c <= '9') | (i > 0 && c == '.' && typeStr.equalsIgnoreCase("float")) && numdecimals<=1)
						{
							save = true;
						}
							
			        	if(real.length()>=10){ // The check for the maximum value
			        		if (c > '2' && i == 0)
			        			save = false;
			        		if (c > '1' && i == 1)
			        			save = false;
			        		if (c > '4' && i == 2)
			        			save = false;
			        		if (c > '7' && i == 3)
			        			save = false;
			        		if (c > '4' && i == 4)
			        			save = false;
			        		if (c > '8' && i == 5)
			        			save = false;
			        		if (c > '3' && i == 6)
			        			save = false;
			        		if (c > '6' && i == 7)
			        			save = false;
			        		if (c > '4' && i == 8)
			        			save = false;
			        		if (c > '7' && i == 9 && offset==0)
			        			save = false;
			        		if (c > '8' && i == 9 && offset==1)	// for negative numbers
			        			save = false;
			        	}
 	        		}
	        	}
				else
				{
					save = false;
				}
	        	if (save && realok && fracok)
	        	{
	    			((Vector<String>) data.get(row)).setElementAt(newValue, col);
	    			fireTableCellUpdated(row,col);
	                if (DEBUG) {
	                    System.out.println("New value of data:");
	                    printDebugData();
	                }
	        	}
				else
				{
					if(typeStr.equalsIgnoreCase("int"))
					{
						JOptionPane.showMessageDialog(null, "Must be a 32 bit integer number from -2,147,483,648 to +2,147,483,647", "Input Error", JOptionPane.ERROR_MESSAGE);
					}
					else
					{
						JOptionPane.showMessageDialog(null, "Must be a 32 bit float number from -2,147,483,648.0 to +2,147,483,647.0", "Input Error", JOptionPane.ERROR_MESSAGE);
					}
						
					return;
				}
	        }
		}
 
		public boolean isCellEditable(int row, int col){
            if (col < 1) {
                return false;
            } else {
				if(data.elementAt(row).toString().contains("int"))
				{
					OPtable.getColumnModel().getColumn(3).setCellEditor(editorInt);
				}else{
					OPtable.getColumnModel().getColumn(3).setCellEditor(editorFloat);					
				}
                return true;
            }
		}
 
		@SuppressWarnings("unchecked")
		public void insertData(Object[] values){
		 	data.add(new Vector<Object>());
		 	for(int i =0; i<values.length; i++){
			 ((Vector<Object>) data.get(data.size()-1)).add(values[i]);
		 	}
		 	fireTableDataChanged();
	 	}
	 
	 	public void removeRow(int row){
	 		data.removeElementAt(row);
	 		fireTableDataChanged();
	 	}
 
        private void printDebugData() {
            int numRows = getRowCount();
            int numCols = getColumnCount();

            for (int i=0; i < numRows; i++) {
                System.out.print("    row " + i + ":");
                for (int j=0; j < numCols; j++) {
                    System.out.print("  " + ((Vector<?>) data.get(i)).elementAt(j) );
                }
                System.out.println();
            }
            System.out.println("----------------------------------------------------");
        }
	} // end of tableModel
	
	//simple function to initialize parameters from the data file to variables
	public static int InitParamsFromFile(String fname, String[] gtnetSktName, String[] gtnetSktType)
	{
		FileInputStream fileInputStream = null;
		InputStreamReader inputStreamReader = null;
		BufferedReader bufferedReader = null;
		String[] var = null;
		int points = 0;
		
		try {
		    String[] result = fileChooser(false,fname);
		    if (result != null){
		    	fileInputStream = new FileInputStream(result[1]);
			    inputStreamReader = new InputStreamReader(fileInputStream, "UTF-8");
			    bufferedReader = new BufferedReader(inputStreamReader);
	
			    // use BufferedReader here
			    String s;
			    
			    while((s = bufferedReader.readLine()) != null){
			        var = s.split(delim);//\t ,\n\r
			        gtnetSktName[points]  = var[0];
			        gtnetSktType[points] = var[1];
			        points++;
			    }
		    }
		} 
		catch (IOException e) {
			e.printStackTrace();
		}
		finally 
		{
			try {
				if (fileInputStream != null){
					fileInputStream.close();
					inputStreamReader.close();
					bufferedReader.close();
				}
			}
			catch (IOException e) {
				e.printStackTrace();
			}		
		}
		return points;
	}	

	//simple function to fill data to table
	public static void fillOutputData(JTable pointTable, int numPoint, String[] gtnetSktName, String[] gtnetSktType)
	{
		OutputTableModel a = (OutputTableModel) pointTable.getModel();
		for (int i=0; i < numPoint; i++) {
			if(gtnetSktType[i].contains("int")){
				Object[] values = {i, gtnetSktName[i], gtnetSktType[i], "1"};
				a.insertData(values);				
			}else{
				Object[] values = {i, gtnetSktName[i], gtnetSktType[i], "9.9"};
				a.insertData(values);				
			}
	    }		
		
	}

	//simple function to fill data to table
	public static void fillInputData(JTable pointTable, int numPoint, String[] gtnetSktName, String[] gtnetSktType)
	{
		InputTableModel a = (InputTableModel) pointTable.getModel();
		for (int i=0; i < numPoint; i++) {
			Object[] values = {i, gtnetSktName[i], gtnetSktType[i], ""};
			a.insertData(values);				
	    }		
		
	}


	//simple function to echo data to terminal
	public static void echo(String msg)
	{
        if (DEBUG) {
        	System.out.println(msg);
        }
        
        textArea.append(msg + newline);

        //Make sure the new text is visible, even if there
        //was a selection in the text area.
        textArea.setCaretPosition(textArea.getDocument().getLength());

	}

	//simple function to save the file name and path
	private static String[] fileChooser(boolean folderOnly, String fname)
	{
		String[] filePath = null;
//		JFileChooser c 	= new JFileChooser();
		myJFileChooser = new JFileChooser();
   	 	File  myfile  = new File(fname);
		
		if(folderOnly)
		{
			myJFileChooser.setDialogTitle("Select Folder...");
			myJFileChooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
			
			int rVal = myJFileChooser.showOpenDialog(null);
			
			if(rVal == JFileChooser.APPROVE_OPTION)
			{
				File folder = myJFileChooser.getSelectedFile();
				
				filePath = new String[2];
				
				filePath[0] = "";
				filePath[1] = folder.getAbsolutePath();
			}
		}
		else
		{
			myJFileChooser.setDialogTitle("Select File");
			myJFileChooser.setSelectedFile(myfile);
			
			int rVal = myJFileChooser.showOpenDialog(null);
			
			if(rVal == JFileChooser.APPROVE_OPTION)
			{
				File fileFolder = myJFileChooser.getSelectedFile();
				
				filePath = new String[2];
				
				filePath[0] = myJFileChooser.getSelectedFile().getName();
				filePath[1] = fileFolder.getAbsolutePath();
			}
		}
		
		return filePath;
	}//end saveAsChooser
	
    public static byte[] convertToByteArray(int value) {
        byte[] bytes = new byte[4];
        ByteBuffer buffer = ByteBuffer.allocate(bytes.length);
        buffer.putInt(value);
        return buffer.array();
    }

    public static byte[] convertToByteArray(float value) {
	    byte[] bytes = new byte[4];
	    ByteBuffer buffer = ByteBuffer.allocate(bytes.length);
	    buffer.putFloat(value);
	    return buffer.array();
	}

    public static byte[] convertToByteArray(byte value) {
        byte[] bytes = new byte[4];
        ByteBuffer buffer = ByteBuffer.allocate(bytes.length);
        buffer.put(value);
        return buffer.array();
    }
    
} // End of Class

class TCP_ListenTask extends Thread 
{
	private volatile boolean stop = false;	
	int npoints = 0;
	int ival[] = null;
	float fval[] = null;
//  ServerSocket serverSocket = null;
//  Socket connectionSocket = null;
	GTNET_SKT_TCP_GUI myskt = null;
  
	public TCP_ListenTask(GTNET_SKT_TCP_GUI myinst, int numPoints) throws SocketException {
	    this.npoints = numPoints;
	    ival = new int[numPoints];
	    fval = new float[numPoints];
	    this.myskt = myinst;
	  }
	public void closeTask() {
      try {
    	  stop = true;
    	  myskt.serverSocket.close();
      } catch (IOException e) {
          e.printStackTrace();
      }
	}
	public void run() {
		while (!stop) {
			ByteBuffer buf;
			try
		    {
		    	//1. creating a server socket, parameter is local port number
			  	myskt.serverSocket = new ServerSocket();
			  	myskt.serverSocket.setReuseAddress(true);
	//			inSocket.setSoTimeout(10000); // DO NOT USE TIMEOUTS
			  	myskt.connectionSocket = new Socket();
		        
		        String ipAddr = GTNET_SKT_TCP_GUI.jTextFieldLocalIp.getText();
		        int ipPort = Integer.parseInt(GTNET_SKT_TCP_GUI.jTextFieldLocalPort.getText());
		        myskt.serverSocket.bind(new InetSocketAddress(ipAddr, ipPort));
		         	         
		        //2. Wait for an incoming data
		        GTNET_SKT_TCP_GUI.echo("Server socket created. Waiting for incoming data...");
		         
		        GTNET_SKT_TCP_GUI.btnTCP.setText("StopServer");
		        GTNET_SKT_TCP_GUI.btnAddIp.setEnabled(false);
		        GTNET_SKT_TCP_GUI.btnDeleteIp.setEnabled(false);
		        GTNET_SKT_TCP_GUI.btnSaveIp.setEnabled(false);
		        GTNET_SKT_TCP_GUI.btnLoadIp.setEnabled(false);
		        GTNET_SKT_TCP_GUI.jTextFieldLocalIp.setEnabled(false);
		        GTNET_SKT_TCP_GUI.jTextFieldLocalPort.setEnabled(false);
		        
	            //communication loop
	            int rxCnt = 0;
	            while (!(myskt.serverSocket.isClosed())) 
		        {
	          		rxCnt++;
	          		
	          		myskt.connectionSocket = myskt.serverSocket.accept();
	          		GTNET_SKT_TCP_GUI.echo("Connection received from " + myskt.connectionSocket.getInetAddress().getHostName() + " : " + myskt.connectionSocket.getPort());
	          		GTNET_SKT_TCP_GUI.btnSend.setEnabled(true);
					InputStream insktStream = myskt.connectionSocket.getInputStream();
					byte[] buff = new byte[(npoints) * 4];                  // Buffer
					int num = -1;                                           // Bytes read     
	                while( (num = insktStream.read(buff)) >= 0 ){           // Not EOS yet                        
	//                	System.out.print(new String(buff,0,num) );          // Echo to console  
	//                	out.write( buff, 0, num );                          // Echo to source                        
	//                	out.flush();                                        // Flush stream                    
	                	if (num==(npoints) * 4)
	                		insktStream.close();							// This is necessary in Server mode to read new packets
	                		break;
	                }   // end while	            
					if (num==(npoints) * 4){
		            	buf = ByteBuffer.wrap(buff);
		          		for (int i = 0; i<npoints;i++)
		          		{
		          			ival[i] = buf.getInt(i*4);
		          			fval[i] = buf.getFloat(i*4);
		          			
		                    //echo the details of incoming data - client ip : client port - client message
		          			if(GTNET_SKT_TCP_GUI.gtnetSktInputType[i].equalsIgnoreCase("int"))
		          			{
		          				GTNET_SKT_TCP_GUI.echo(myskt.connectionSocket.getInetAddress().getHostAddress() + ":" + myskt.connectionSocket.getPort() + " - Point" + String.valueOf(i) + " Integer = " + String.valueOf(ival[i]) + "     - Packets Received="+String.valueOf(rxCnt));         				
		          				GTNET_SKT_TCP_GUI.IPtable.setValueAt(String.valueOf(ival[i]), i, 3);
		           			}
		          			if(GTNET_SKT_TCP_GUI.gtnetSktInputType[i].equalsIgnoreCase("float"))
		          			{
		          				GTNET_SKT_TCP_GUI.echo(myskt.connectionSocket.getInetAddress().getHostAddress() + ":" + myskt.connectionSocket.getPort() + " - Point" + String.valueOf(i) + " Float   = " + String.valueOf(fval[i]) + " - Packets Received="+String.valueOf(rxCnt));
		          				GTNET_SKT_TCP_GUI.IPtable.setValueAt(String.valueOf(fval[i]), i, 3);
		           			}
		          		}
	         		}	         	          		
		        }		 
			} catch (UnknownHostException ex) {
				ex.printStackTrace();
				GTNET_SKT_TCP_GUI.echo("UnknownHostException:Server: " + ex.getMessage() + "\n");
			} catch (SocketException ex) {
				ex.printStackTrace();
				GTNET_SKT_TCP_GUI.echo("SocketException:Server: " + ex.getMessage() + "\n");
				GTNET_SKT_TCP_GUI.btnSend.setEnabled(false);
			} catch (IOException ex) {
				ex.printStackTrace();
				GTNET_SKT_TCP_GUI.echo("IOException:Server: " + ex.getMessage() + "\n");
			}
			catch(Exception ex) {
				GTNET_SKT_TCP_GUI.echo("Exception:Server: " + ex.getMessage() + "\n");
			}        
		}
	    if (stop)
	        System.out.println("TCP_ListenTask Closed Now");
	}
}	

class TCP_ConnectTask extends Thread 
{
	
  int npoints = 0;
  int ival[] = null;
  float fval[] = null;
  GTNET_SKT_TCP_GUI myskt = null;
  
  public TCP_ConnectTask(GTNET_SKT_TCP_GUI myinst, int numPoints) throws SocketException {
	    this.npoints = numPoints;
	    ival = new int[numPoints];
	    fval = new float[numPoints];
	    this.myskt = myinst;
	  }
  public void closeTask() {
      try {
    	  myskt.connectionSocket.close();
    	  GTNET_SKT_TCP_GUI.btnTCP.setText("Connect");
    	  GTNET_SKT_TCP_GUI.jTextFieldRemoteIp.setEnabled(true);
    	  GTNET_SKT_TCP_GUI.jTextFieldRemotePort.setEnabled(true);
    	  GTNET_SKT_TCP_GUI.btnSend.setEnabled(false);
      } catch (IOException e) {
          e.printStackTrace();
      }
      System.out.println("TCP_ConnectTask Closed Now");
      GTNET_SKT_TCP_GUI.echo("TCP_ConnectTask Closed Now");
  }
  public void run() {
//		int size = npoints * 4;
		  
		try {
		    InetAddress gtnetIp = InetAddress.getByName(GTNET_SKT_TCP_GUI.jTextFieldRemoteIp.getText());
		    int gtnetPort = Integer.parseInt(GTNET_SKT_TCP_GUI.jTextFieldRemotePort.getText());
		    
			myskt.connectionSocket = new Socket(gtnetIp, gtnetPort);
			myskt.connectionSocket.setKeepAlive(true);
			myskt.connectionSocket.setTcpNoDelay(true);
			
//			DataOutputStream outToServer = new DataOutputStream(myskt.connectionSocket.getOutputStream());
//			BufferedReader inFromServer = new BufferedReader(new InputStreamReader(myskt.connectionSocket.getInputStream()));

			GTNET_SKT_TCP_GUI.btnSend.setEnabled(true);
			GTNET_SKT_TCP_GUI.btnTCP.setText("StopClient");
			GTNET_SKT_TCP_GUI.echo("Client socket opened and ready...");
			GTNET_SKT_TCP_GUI.jTextFieldRemoteIp.setEnabled(false);
			GTNET_SKT_TCP_GUI.jTextFieldRemotePort.setEnabled(false);
	    
            //communication loop
            int rxCnt = 0;
            while (!(myskt.connectionSocket.isClosed())) 
	        {
          		rxCnt++;
          		
          		GTNET_SKT_TCP_GUI.echo("Packets received from " + myskt.connectionSocket.getInetAddress().getHostName() + " : " + myskt.connectionSocket.getPort());
				InputStream inskt = myskt.connectionSocket.getInputStream();
				byte[] buff = new byte[(npoints) * 4];                  // Buffer
				int num = -1;                                           // Bytes read  
				if ((num = inskt.read(buff))== -1){
					GTNET_SKT_TCP_GUI.echo("TCP connection likely lost - No input data read");
					closeTask();
				}
//	                while( (num = inskt.read(buff)) >= 0 ){                 // Not EOS yet                        
//	//                	System.out.print(new String(buff,0,num) );          // Echo to console  
//	//                	out.write( buff, 0, num );                          // Echo to source                        
//	//                	out.flush();                                        // Flush stream                    
//	                	if (num==(npoints) * 4)
//	//                		inskt.close();
//	                		break;
//	                }   // end while
               
				if (num==(npoints) * 4){
					ByteBuffer buf = ByteBuffer.wrap(buff);
	          		for (int i = 0; i<npoints;i++)
	          		{
	          			ival[i] = buf.getInt(i*4);
	          			fval[i] = buf.getFloat(i*4);
	          			
	                    //echo the details of incoming data - client ip : client port - client message
	          			if(GTNET_SKT_TCP_GUI.gtnetSktInputType[i].equalsIgnoreCase("int"))
	          			{
	          				GTNET_SKT_TCP_GUI.echo(myskt.connectionSocket.getInetAddress().getHostAddress() + ":" + myskt.connectionSocket.getPort() + " - Point" + String.valueOf(i) + " Integer = " + String.valueOf(ival[i]) + "     - Packets Received="+String.valueOf(rxCnt));         				
	          				GTNET_SKT_TCP_GUI.IPtable.setValueAt(String.valueOf(ival[i]), i, 3);
	           			}
	          			if(GTNET_SKT_TCP_GUI.gtnetSktInputType[i].equalsIgnoreCase("float"))
	          			{
	          				GTNET_SKT_TCP_GUI.echo(myskt.connectionSocket.getInetAddress().getHostAddress() + ":" + myskt.connectionSocket.getPort() + " - Point" + String.valueOf(i) + " Float   = " + String.valueOf(fval[i]) + " - Packets Received="+String.valueOf(rxCnt));
	          				GTNET_SKT_TCP_GUI.IPtable.setValueAt(String.valueOf(fval[i]), i, 3);
	           			}
	         		}	         	          		
				}
	        }
			
		} catch (UnknownHostException ex) {
			ex.printStackTrace();
			GTNET_SKT_TCP_GUI.echo("UnknownHostException:Client: " + ex.getMessage() + "\n");
		} catch (SocketException ex) {
			ex.printStackTrace();
			GTNET_SKT_TCP_GUI.echo("SocketException:Client: " + ex.getMessage() + "\n");
			GTNET_SKT_TCP_GUI.btnSend.setEnabled(false);
		} catch (IOException ex) {
			ex.printStackTrace();
			GTNET_SKT_TCP_GUI.echo("IOException:Client: " + ex.getMessage() + "\n");
		}
		catch(Exception ex) {
			GTNET_SKT_TCP_GUI.echo("Exception:Client: " + ex.getMessage() + "\n");
		}        
    }
}	

	