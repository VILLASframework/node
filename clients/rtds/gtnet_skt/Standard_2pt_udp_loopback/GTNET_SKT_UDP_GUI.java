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
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
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
import javax.swing.SwingWorker;
import javax.swing.UIManager;
import javax.swing.border.CompoundBorder;
import javax.swing.border.EmptyBorder;
import javax.swing.border.TitledBorder;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableCellEditor;

/*
 * NOTE: GTNET_SKT_UDP was made as an example UDP client/server
 * 		 The code is given as an example and is not intended to be 
 * 		 part of RSCAD support. Questions will not be answered.
 */

public class GTNET_SKT_UDP_GUI extends JFrame
{
    /**
	 * 
	 */
	private static final long serialVersionUID = -6325275282238226823L;

	private static boolean DEBUG = false;

    static UDP_ServerTask process;
	//UDP Server
	static JTextField jTextFieldLocalIp;
	static JTextField jTextFieldLocalPort;
	public static SoftEdgeButton btnStart;
	public static SoftEdgeButton btnQuit;
	public static SoftEdgeButton btnAddIp;
	public static SoftEdgeButton btnDeleteIp;
	public static SoftEdgeButton btnSaveIp;
	public static SoftEdgeButton btnLoadIp;
	static JTable IPtable;
	//UDP Client
	static JTextField jTextFieldRemoteIp;
	static JTextField jTextFieldRemotePort;
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
	static DatagramSocket inSocket = null;
    static DatagramPacket inPacket = null; //receiving packet
    static DatagramPacket outPacket = null; //sending packet
    static byte[] inBuf = null;
	static byte[] outBuf = null;
    static String msg = null;
    public static TableCellEditor editorInt;
    public static TableCellEditor editorFloat;
    protected static JTextArea textArea;
    private final static String newline = "\r\n";
    
	
    public GTNET_SKT_UDP_GUI()
    {
        super("RTDS GTNET-SKT UDP Server and Client");        
        
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

		setSize(555,745);
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
		MainPnl.add(createInputPointPnl(), gbc);
		gbc.gridx = 0;
		gbc.gridy = 1;
		gbc.gridwidth = 1;
		MainPnl.add(createOutputPointPnl(), gbc);
		gbc.gridx = 0;
		gbc.gridy = 2;
		gbc.gridwidth = 1;
		MainPnl.add(createMsgPnl(), gbc);
			  	
    }

	private static JPanel createInputPointPnl() {
		
		//Points from GTNET-SKT
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "GTNET-SKT UDP Server");
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
		
		jTextFieldLocalIp = new JTextField("", 10);
		jTextFieldLocalIp.setText(host.getHostAddress());
		jTextFieldLocalIp.setEditable(true);
		
		jTextFieldLocalPort = new JTextField("", 6);
        jTextFieldLocalPort.setText("12345");

		btnStart= new SoftEdgeButton("Start");
		btnAddIp = new SoftEdgeButton("Add") ;
		btnDeleteIp = new SoftEdgeButton("Delete") ;
	    btnSaveIp = new SoftEdgeButton("Save Table");   
	    btnLoadIp = new SoftEdgeButton("Load Table");   

		btnStart.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
			    if(GTNET_SKT_UDP_GUI.btnStart.getText()=="Start")
			    {
			    	GTNET_SKT_UDP_GUI.btnStart.setText("Stop");
			        GTNET_SKT_UDP_GUI.btnAddIp.setEnabled(false);
			        GTNET_SKT_UDP_GUI.btnDeleteIp.setEnabled(false);
			        GTNET_SKT_UDP_GUI.btnSaveIp.setEnabled(false);
			        GTNET_SKT_UDP_GUI.btnLoadIp.setEnabled(false);
			    	GTNET_SKT_UDP_GUI.jTextFieldLocalIp.setEnabled(false);
			    	GTNET_SKT_UDP_GUI.jTextFieldLocalPort.setEnabled(false);
//			        GTNET_SKT_UDP_GUI.jTextFieldRemoteIp.setEnabled(false);
//			        GTNET_SKT_UDP_GUI.jTextFieldRemotePort.setEnabled(false);
			        try {
			        	GTNET_SKT_UDP_GUI.process = new UDP_ServerTask(GTNET_SKT_UDP_GUI.numInPoints);
			        	GTNET_SKT_UDP_GUI.process.execute();
			        } 
			        catch (Exception e1) 
			        {
			        	echo(e1.getMessage());
			        }
			    }
			    else
			    {
			    	GTNET_SKT_UDP_GUI.btnStart.setText("Start");
			        GTNET_SKT_UDP_GUI.btnAddIp.setEnabled(true);
			        GTNET_SKT_UDP_GUI.btnDeleteIp.setEnabled(true);
			        GTNET_SKT_UDP_GUI.btnSaveIp.setEnabled(true);
			        GTNET_SKT_UDP_GUI.btnLoadIp.setEnabled(true);
			    	GTNET_SKT_UDP_GUI.jTextFieldLocalIp.setEnabled(true);
			    	GTNET_SKT_UDP_GUI.jTextFieldLocalPort.setEnabled(true);
//			        GTNET_SKT_UDP_GUI.jTextFieldRemoteIp.setEnabled(true);
//			        GTNET_SKT_UDP_GUI.jTextFieldRemotePort.setEnabled(true);
			    	GTNET_SKT_UDP_GUI.process.closeTask();
			    	GTNET_SKT_UDP_GUI.process.cancel(true);
			    	GTNET_SKT_UDP_GUI.echo("Server socket closed.");			        
			    }				
			}			
		});

		btnAddIp.addActionListener(new ActionListener() {
			@Override
		     public void actionPerformed(ActionEvent e) 
		     {
				if(GTNET_SKT_UDP_GUI.numInPoints< GTNET_SKT_UDP_GUI.GTNETSKT_MAX_INPUTS){
					int i = GTNET_SKT_UDP_GUI.numInPoints++;
					InputTableModel a = (InputTableModel) GTNET_SKT_UDP_GUI.IPtable.getModel();
					GTNET_SKT_UDP_GUI.gtnetSktInputName[i] = "POINTin" + Integer.toString(i);
					GTNET_SKT_UDP_GUI.gtnetSktInputType[i] = "float";
					Object[] values = {i, GTNET_SKT_UDP_GUI.gtnetSktInputName[i], GTNET_SKT_UDP_GUI.gtnetSktInputType[i], ""};
					a.insertData(values);
				}else{
					GTNET_SKT_UDP_GUI.echo("Number of Server Points is at maximum of:" + GTNET_SKT_UDP_GUI.GTNETSKT_MAX_INPUTS);
				}				
		     }			
		});
		
		btnDeleteIp.addActionListener(new ActionListener(){
			@Override
			public void actionPerformed(ActionEvent e) {
		    	 if(GTNET_SKT_UDP_GUI.IPtable.getSelectedRow() != -1){
		    		 GTNET_SKT_UDP_GUI.InputTableModel a = (GTNET_SKT_UDP_GUI.InputTableModel) GTNET_SKT_UDP_GUI.IPtable.getModel();
		    		 a.removeRow(GTNET_SKT_UDP_GUI.IPtable.getSelectedRow());
		    		 GTNET_SKT_UDP_GUI.numInPoints--;
		    		 for (int i=0; i < GTNET_SKT_UDP_GUI.numInPoints; i++) {
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
		
		JPanel LocalPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(2, 2, 2, 2);
		JLabel lblLocalIp = new JLabel("Local Server Settings, IP Address:");
		LocalPanel.add(lblLocalIp, gbc);
		LocalPanel.add(jTextFieldLocalIp, gbc);
		LocalPanel.add(new JLabel("Port:"), gbc);
		LocalPanel.add(jTextFieldLocalPort, gbc);
		LocalPanel.add(btnStart, gbc);
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
		pnlPoint.add(LocalPanel, gbc);
		gbc.gridx = 0;
		gbc.gridy = 2;
		pnlPoint.add(tableButtonPanel, gbc);

		return pnlPoint;
		
	}
	
	private static JPanel createOutputPointPnl() {
		
		TitledBorder borderPoints = BorderFactory.createTitledBorder(new SoftEdgeBorder(3,3, new Color(213, 223, 229)), "GTNET-SKT UDP Client");

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
		
		jTextFieldRemoteIp = new JTextField("", 9);
		jTextFieldRemoteIp.setText(host.getHostAddress());
        if (DEBUG) {
    		jTextFieldRemoteIp.setText("172.24.9.185");
        }
		jTextFieldRemoteIp.setEditable(true);
		
		jTextFieldRemotePort = new JTextField("", 5);
        jTextFieldRemotePort.setText("12345");

		btnSend= new SoftEdgeButton("Send");
		SoftEdgeButton btnAddOp = new SoftEdgeButton("Add") ;
		SoftEdgeButton btnDeleteOp = new SoftEdgeButton("Delete") ;
	    SoftEdgeButton btnSaveOp = new SoftEdgeButton("Save Table");   
	    SoftEdgeButton btnLoadOp = new SoftEdgeButton("Load Table");   

		btnSend.addActionListener(new ActionListener() {   
			@Override
			public void actionPerformed(ActionEvent e) {
				DatagramSocket outSocket = null;
				int size = GTNET_SKT_UDP_GUI.numOutPoints * 4;
				try {
				    InetAddress gtnetIp = InetAddress.getByName(GTNET_SKT_UDP_GUI.jTextFieldRemoteIp.getText());
				    int gtnetPort = Integer.parseInt(GTNET_SKT_UDP_GUI.jTextFieldRemotePort.getText());
				    byte[] buffer = new byte[size];
				    ByteBuffer outbuff =  ByteBuffer.wrap(buffer);
				    
		      		for (int i = 0; i<GTNET_SKT_UDP_GUI.numOutPoints;i++)
		      		{
		      			if(GTNET_SKT_UDP_GUI.gtnetSktOutputType[i].equalsIgnoreCase("int"))
		      			{	    
		      				//byte[] src = GTNET_SKT_UDP.convertToByteArray(Integer.valueOf((String) GTNET_SKT_UDP.OPtable.getValueAt(i, 3))); 
		      				// Note: if input is sent as float we can just round and the return will always be an INT so no exception will be thrown as with above line of code
			      			byte[] src = GTNET_SKT_UDP_GUI.convertToByteArray(Math.round(Float.valueOf((String) GTNET_SKT_UDP_GUI.OPtable.getValueAt(i, 3))));  
			      			outbuff.put(src, 0, 4);
		      			}
		      			if(GTNET_SKT_UDP_GUI.gtnetSktOutputType[i].equalsIgnoreCase("float"))
		      			{
			      			byte[] src = GTNET_SKT_UDP_GUI.convertToByteArray(Float.valueOf((String) GTNET_SKT_UDP_GUI.OPtable.getValueAt(i, 3))); 
			      			outbuff.put(src, 0, 4);
		      			}
		      		}
		      		buffer = outbuff.array();
				    DatagramPacket outPacket = new DatagramPacket(buffer, size, gtnetIp, gtnetPort);
				    outSocket = new DatagramSocket();
				    outSocket.send(outPacket);
				    
				} catch (UnknownHostException ex) {
					ex.printStackTrace();
				} catch (SocketException ex) {
					ex.printStackTrace();
				} catch (IOException ex) {
					ex.printStackTrace();
				}
				outSocket.close();				
			}
        });
		
		btnAddOp.addActionListener(new ActionListener() {
			@Override
		    public void actionPerformed(ActionEvent e) 
		    {
				if(GTNET_SKT_UDP_GUI.numOutPoints< GTNET_SKT_UDP_GUI.GTNETSKT_MAX_INPUTS){
					int i = GTNET_SKT_UDP_GUI.numOutPoints++;
					GTNET_SKT_UDP_GUI.OutputTableModel a = (GTNET_SKT_UDP_GUI.OutputTableModel) GTNET_SKT_UDP_GUI.OPtable.getModel();
					GTNET_SKT_UDP_GUI.gtnetSktOutputName[i] = "POINT" + Integer.toString(i);
					GTNET_SKT_UDP_GUI.gtnetSktOutputType[i] = "float";
					Object[] values = {i, GTNET_SKT_UDP_GUI.gtnetSktOutputName[i], GTNET_SKT_UDP_GUI.gtnetSktOutputType[i], "9.9"};
					a.insertData(values);
				}else{
					GTNET_SKT_UDP_GUI.echo("Number of Client Points is at maximum of:" + GTNET_SKT_UDP_GUI.GTNETSKT_MAX_INPUTS);
				}
		    }			
		});
		
		btnDeleteOp.addActionListener(new ActionListener() {
			@Override
		    public void actionPerformed(ActionEvent e) 
		    {
		    	 if(GTNET_SKT_UDP_GUI.OPtable.getSelectedRow() != -1){
		    		 GTNET_SKT_UDP_GUI.OutputTableModel a = (GTNET_SKT_UDP_GUI.OutputTableModel) GTNET_SKT_UDP_GUI.OPtable.getModel();
		    		 a.removeRow(GTNET_SKT_UDP_GUI.OPtable.getSelectedRow());
		    		 GTNET_SKT_UDP_GUI.numOutPoints--;
		    		 for (int i=0; i < GTNET_SKT_UDP_GUI.numOutPoints; i++) {
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
		
		JPanel remotePanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
		gbc = new GridBagConstraints();		
		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 0;
	    gbc.insets = new Insets(1, 1, 1, 1);
		JLabel lblLocalIp = new JLabel("Remote Server Settings, IP Address:");
		remotePanel.add(lblLocalIp, gbc);
		remotePanel.add(jTextFieldRemoteIp, gbc);
		remotePanel.add(new JLabel("Port:"), gbc);
		remotePanel.add(jTextFieldRemotePort, gbc);
		remotePanel.add(btnSend, gbc);

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

		gbc.fill = GridBagConstraints.NONE;
		gbc.gridx = 0;
		gbc.gridy = 1;
		pnlPoint.add(remotePanel, gbc);
		gbc.gridx = 0;
		gbc.gridy = 2;
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
		if (fileChooser(false,".txt") !=  null) {   
            saveOPTable(myJFileChooser.getSelectedFile().toString());   
        }   
    }   
    private static void saveOPTable(String myfile) {   
        try {   
        	StringBuffer fileContent = new StringBuffer();
        	OutputTableModel tModel = (OutputTableModel) OPtable.getModel();
        	for (int i = 0; i < tModel.getRowCount(); i++) {

        	     Object cellValue = tModel.getValueAt(i, 1) + "\t" + tModel.getValueAt(i, 2) + newline;
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
		if (fileChooser(false,".txt") !=  null) {   
			saveIPTable(myJFileChooser.getSelectedFile().toString());   
		}   
    }   
    private static void saveIPTable(String myfile) {   
        try {   
        	StringBuffer fileContent = new StringBuffer();
        	InputTableModel tModel = (InputTableModel) IPtable.getModel();
        	for (int i = 0; i < tModel.getRowCount(); i++) {

        	     Object cellValue = tModel.getValueAt(i, 1) + "\t" + tModel.getValueAt(i, 2) + newline ;
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
            i = InitParamsFromFile(".txt",gtnetSktOutputName,gtnetSktOutputType);
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
        	i = InitParamsFromFile(".txt",gtnetSktInputName,gtnetSktInputType);
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
            }else if(GTNET_SKT_UDP_GUI.btnStart.getText()=="Stop"){
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
	 	
	public static void main(String args[])
	{
		new GTNET_SKT_UDP_GUI();
		
		fillInputData(IPtable,numInPoints,gtnetSktInputName,gtnetSktInputType);	//Server Data Point File
		
		fillOutputData(OPtable, numOutPoints, gtnetSktOutputName, gtnetSktOutputType);	//Client Data Point File
	}
	
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
			        var = s.split("\t");//\t ,\n\r
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


} // End of Class


/*
 * CancelListener
 */
class CancelListener implements ActionListener 
{        
     public void actionPerformed(ActionEvent e) 
     {
    	 System.exit(0);
     }
 }


class UDP_ServerTask extends SwingWorker<Object, Object> 
{
  int npoints = 0;
  int ival[] = null;
  float fval[] = null;

  public UDP_ServerTask(int numPoints) throws SocketException {
	    this.npoints = numPoints;
	    ival = new int[numPoints];
	    fval = new float[numPoints];
	  }
  public void closeTask() {
      GTNET_SKT_UDP_GUI.inSocket.close();
      System.out.println("UDP_ServerTask Closed Now");
      GTNET_SKT_UDP_GUI.echo("UDP_ServerTask Closed Now");
  }
  @Override
  protected Object doInBackground() throws Exception {
	  ByteBuffer buf;
	  byte[] data = null;
	  byte[] inBuf;
	  
	  	    
 	    try
	    {
	    	//1. creating a server socket, parameter is local port number
 	    	GTNET_SKT_UDP_GUI.inSocket = new DatagramSocket(null);
 	    	GTNET_SKT_UDP_GUI.inSocket.setReuseAddress(true);
	        
	        String ipAddr = GTNET_SKT_UDP_GUI.jTextFieldLocalIp.getText();
	        int localPort = Integer.parseInt(GTNET_SKT_UDP_GUI.jTextFieldLocalPort.getText());
	        GTNET_SKT_UDP_GUI.inSocket.bind(new InetSocketAddress(ipAddr, localPort));
	         
	        //buffer to receive incoming data
	        inBuf = new byte[(npoints) * 4]; 
	        DatagramPacket inPacket = new DatagramPacket(inBuf, inBuf.length);
	         
	        //2. Wait for an incoming data
	        GTNET_SKT_UDP_GUI.echo("Server socket created. Waiting for incoming data...");
	         
            //communication loop
            int rxCnt = 0;
	        while(!isCancelled())
	        {
          		rxCnt++;		

          		GTNET_SKT_UDP_GUI.inSocket.receive(inPacket);
                data = inPacket.getData();
                buf = ByteBuffer.wrap(data); 
          		for (int i = 0; i<npoints;i++)
          		{
          			ival[i] = buf.getInt(i*4);
          			fval[i] = buf.getFloat(i*4);
          			
                    //echo the details of incoming data - client ip : client port - client message
          			if(GTNET_SKT_UDP_GUI.gtnetSktInputType[i].equalsIgnoreCase("int"))
          			{
          				GTNET_SKT_UDP_GUI.echo(inPacket.getAddress().getHostAddress() + ":" + inPacket.getPort() + " - Point" + String.valueOf(i) + " Integer = " + String.valueOf(ival[i]) + "     - Packets Received="+String.valueOf(rxCnt));         				
          				GTNET_SKT_UDP_GUI.IPtable.setValueAt(String.valueOf(ival[i]), i, 3);
           			}
          			if(GTNET_SKT_UDP_GUI.gtnetSktInputType[i].equalsIgnoreCase("float"))
          			{
          				GTNET_SKT_UDP_GUI.echo(inPacket.getAddress().getHostAddress() + ":" + inPacket.getPort() + " - Point" + String.valueOf(i) + " Float   = " + String.valueOf(fval[i]) + " - Packets Received="+String.valueOf(rxCnt));
          				GTNET_SKT_UDP_GUI.IPtable.setValueAt(String.valueOf(fval[i]), i, 3);
           			}
         		}	         	          		
	        }		 
//	        GTNET_SKT_UDP_GUI.inSocket.close();
	    }	              
 	    catch (UnknownHostException ex) 
 	    {
	  	   System.err.println(ex);	          
	    } 
 	    catch (SocketException ex) 
 	    {
	      	System.err.println(ex);	            
	    } 
 	    catch (IOException ex) 
 	    {
	      	System.err.println(ex);	            
	    }	 	    
      return null;
    }
}	
	