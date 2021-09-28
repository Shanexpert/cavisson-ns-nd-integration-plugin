/**---------------------------------------------------------------------------/
 * Name       SchAddGeneratorGUI.java
 * Purpose    This file will open a dialog to add or Edit Generator(s) for the whole scenario.
 * @author    Manish kumar Gupta
 *---------------------------------------------------------------------------**/

package pacScenario;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ComponentEvent;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.net.URL;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Vector;

import javax.swing.BorderFactory;
import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.event.TableModelEvent;
import javax.swing.event.TableModelListener;
import javax.swing.table.DefaultTableCellRenderer;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableCellEditor;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;
import javax.swing.table.TableColumnModel;
import javax.swing.table.TableModel;
import javax.swing.table.TableRowSorter;

import pac1.Bean.Log;
import pac1.Bean.CmdExec;
import pac1.Bean.FileBean;
import pac1.Bean.Log;
import pac1.Bean.rptUtilsBean;
import pacServletClient.ServletReport;
import pac1.Bean.NSColor;
import pac1.Bean.ScriptNode;
import pac1.Bean.nsConfig;

class SchAddGeneratorGUI implements ActionListener
{
  private String className = "SchAddGeneratorGUI";
  private JButton btnOk; // for button ok
  private JButton btnCancel; // for button cancel
  private JDialog dialog; // for JDialog window
  private Object colName[] = null; // for columns of JTable
  private JButton btnBrowseFile; // for Browse generator File.
  private JTable fileDataArea; // for showing Generator Data
  private File file; // for storing Generator File
  private StringBuffer buffer; // for storing buffer data of generator file
  private String arrFiledata[][] = null; // initialize file data as null
  private JCheckBox chkUseAllGen; // for use all generator for all scenario groups
  private MyTableModel tableModel; //
  private String StrGenFilePath = " "; // for generator filePath
  private ArrayList ListOfGroupNames = new ArrayList(); // for holding list of available groups in the scenario.
  private SchScheduleData schScheduleData;
  private SchScenGroup schScenGroup;
  private SchUtility util;
  private SchMain schMain;
  private FileBean fileBean = new FileBean("test_dummy");
  private boolean isEditOrAdd = false; // boolean value used to check whether screen to be opened in Add or Edit Mode.
  public String ListOfSelectedGeneratorIs = ""; // for storing list of generators selected for the whole scenario.
  public JTextField txtGnName; // for showing selected generator file path.
  private JTable creatingTable;
  static boolean isNewFileDownload = false; // used to check whether new generator file is downloaded
  String[] ArrNewSelectedGenerator; // used for holding array of new selected Generator(s)
  private JCheckBox chkAccessToExternal; // this is used to tell user whether we have access to external generators or not
  private JCheckBox chkDistributeLoad; //this is for Distributing load between different generator
  
  public SchAddGeneratorGUI(SchScheduleData schScheduleData, SchUtility schUtility, SchScenGroup schScenGroup, boolean isEditOrAdd)
  {
    Log.debugLog(className, "SchAddGeneratorGUI", "", "", "Constructor called");
    this.schScheduleData = schScheduleData; // holding reference of SchScheduleData
    this.util = schUtility; // holding reference of SchUtility
    this.schScenGroup = schScenGroup; // holding reference of SchScenGroup
    this.isEditOrAdd = isEditOrAdd;// hoding boolean value for AddorEdit
  }

  public SchAddGeneratorGUI(SchScheduleData schScheduleData, boolean isEditOrAdd)
  {
    Log.debugLog(className, "SchAddGeneratorGUI", "", "", "Constructor called");
    this.schScheduleData = schScheduleData; // holding reference of SchScheduleData
    this.isEditOrAdd = isEditOrAdd;// hoding boolean value for AddorEdit
  }

  /* this method is used to create dialog for Add GeneratorGUI.
   * And setting upper, middle and lower panel to the dialog.
   *
   */
  public void showWindow()
  {
    Log.debugLog(className, "showWindow", "", "", "Method Start");
    JOptionPane optionPane = new JOptionPane();
    optionPane = new JOptionPane();
    optionPane.removeAll();
    String str = "Add Generator(s)"; //setting title of dialog window
    if(!isEditOrAdd)
      str = "Edit Generator(s)"; //setting title of dialog window

    dialog = optionPane.createDialog(null, str); // creating dialog
    optionPane.setLayout(new BorderLayout(5, 5)); //setting layout
    optionPane.add(createUpperPanel(), BorderLayout.NORTH); //setting upper panel
    optionPane.add(createMiddlePanel(), BorderLayout.CENTER); //setting middle panel
    optionPane.add(createLowerBtnPanel(), BorderLayout.SOUTH); //setting lower button panel
    setScreenSize(70,55);
    if(schScheduleData.getControllerType().equals("Internal"))
      setScreenSize(70,55);
    if(!isEditOrAdd)
      setScreenSize(60,50);
    dialog.setResizable(true);
    URL imageURL = null;
    try
    {
      imageURL = new URL(schMain.urlCodeBase + "../images/logo_TitleIcon.png");
    }
    catch(Exception e)
    {
      Log.stackTraceLog(className, "imageURL", "", "", "Exception in imageURL", e);
    }

    ((Frame)dialog.getOwner()).setIconImage(Toolkit.getDefaultToolkit().getImage(imageURL));
    dialog.addWindowListener(new WindowAdapter()
    {
      public void windowClosing(WindowEvent w) // setting window close event to dialog window
      {
        dialog.dispose();
      }
    });
    dialog.setVisible(true);
  }

  /* this method is used to set Generator screen size.
   *
   */
  public void setScreenSize(int width, int height)
  {
    //Get the screen size
    Toolkit toolkit = Toolkit.getDefaultToolkit();
    Dimension screenSize = toolkit.getScreenSize();

    //Calculate the frame location
    int screenWidth = (screenSize.width * width)/ 100;
    int screenHeight = (screenSize.height * height)/ 100;
    int x = (screenSize.width - screenWidth)/2;
    int y = (screenSize.height - screenHeight)/2;

    dialog.setSize(screenWidth, screenHeight);
    dialog.addComponentListener(new java.awt.event.ComponentAdapter()
    {
      public void componentResized(ComponentEvent event)
      {
        dialog.setSize(Math.max(100, dialog.getWidth()),Math.max(100, dialog.getHeight()));
      }
    });

    //Set the new frame location
    dialog.setLocation(x, y);
  }

  /* this method is used to create upper panel showing checkbox for Use Generator,
   * Label for Select Generator File,
   * TextField for Showing selected
   * Button for Browse Generator File.
   */
  private JPanel createUpperPanel()
  {
    Log.debugLog(className, "createUpperPanel", "", "", "Method Start");
    JPanel centerPanal = new JPanel();
    GridBagLayout gridBagLayout = new GridBagLayout();
    gridBagLayout.columnWidths = new int[]{0, 8};
    centerPanal.setLayout(gridBagLayout); // setting layout

    if(isEditOrAdd == true)
    {
      JLabel labelGrp = new JLabel(); // setting Label for Select Generator File
      labelGrp.setText("Select Generator(s) File:");
      GridBagConstraints gridBagConstraints_Label = new GridBagConstraints();
      gridBagConstraints_Label.anchor = GridBagConstraints.WEST;
      gridBagConstraints_Label.insets = new Insets(5, 0, 10, 10);
      gridBagConstraints_Label.gridx = 0;
      gridBagConstraints_Label.gridy = 0;
      //centerPanal.add(labelGrp, gridBagConstraints_Label);

      txtGnName = new JTextField(); // setting textfield for showing selected generator file.
      txtGnName.setEditable(false);
      GridBagConstraints gridBagConstraints_txt = new GridBagConstraints();
      gridBagConstraints_txt.anchor = GridBagConstraints.WEST;
      gridBagConstraints_txt.insets = new Insets(0, 0, 10, 10);
      gridBagConstraints_txt.ipadx = 150;
      gridBagConstraints_txt.weightx = 1.0;
      gridBagConstraints_txt.weighty = 4.0;
      gridBagConstraints_txt.gridx = 1;
      gridBagConstraints_txt.gridy = 0;
      gridBagConstraints_txt.gridwidth = 2;
      gridBagConstraints_txt.fill = GridBagConstraints.HORIZONTAL;
     // centerPanal.add(txtGnName, gridBagConstraints_txt);

      btnBrowseFile = new JButton("Browse"); // setting button to Browse Generator file
      GridBagConstraints gridBagConstraints_btn = new GridBagConstraints();
      gridBagConstraints_btn.anchor = GridBagConstraints.WEST;
      gridBagConstraints_btn.insets = new Insets(0, 0, 10, 10);
      gridBagConstraints_btn.weightx = 16.0;
      gridBagConstraints_btn.weighty = 4.0;
      gridBagConstraints_btn.gridx = 3;
      gridBagConstraints_btn.gridy = 0;
      btnBrowseFile.setActionCommand("browse");
      btnBrowseFile.addActionListener(this);
      //centerPanal.add(btnBrowseFile, gridBagConstraints_btn);

      JLabel SelectGenerator = new JLabel(); // setting label for selecting generator
      SelectGenerator.setText("Select Generator(s)");
      GridBagConstraints gridBagConstraints_3 = new GridBagConstraints();
      gridBagConstraints_3.anchor = GridBagConstraints.WEST;
      gridBagConstraints_3.insets = new Insets(0, 0, 10, 10);
      gridBagConstraints_3.gridx = 0;
      gridBagConstraints_3.gridy = 2;
      gridBagConstraints_3.weightx = 1.0;
      gridBagConstraints_3.weighty = 1.0;
      gridBagConstraints_3.ipady = 0;
      centerPanal.add(SelectGenerator, gridBagConstraints_3);

     /* if(schScheduleData.getFilePath() == "")
      {
        txtGnName.setText("Please Browse a Generator(s) File.");
        file = null;
      }
      else
      {
        txtGnName.setText(schScheduleData.getFilePath());
        file = new File(schScheduleData.getFilePath());
      }
    */
    }
    else
    {
    //Adding CheckBox for Distribute load
      GridBagConstraints distribute_chk = new GridBagConstraints();
      distribute_chk.anchor = GridBagConstraints.WEST;
      distribute_chk.insets = new Insets(0, 0, 10, 10);
      distribute_chk.weightx = 16.0;
      distribute_chk.weighty = 4.0;
      distribute_chk.gridx = 0;
      distribute_chk.gridy = 0;
      chkDistributeLoad = new JCheckBox("Distributed Load Equally to all Generators");
      chkDistributeLoad.setSelected(false);
      chkDistributeLoad.addItemListener(new ItemListener()
      {
        @Override
        public void itemStateChanged(ItemEvent e)
        {
          updateDistributeLoadPctVal();
        }
      });
      centerPanal.add(chkDistributeLoad, distribute_chk);
      
      
      JLabel SelectGenerator = new JLabel(); // initializing lable
      SelectGenerator.setText("Select Generator(s) from the Available list:");
      GridBagConstraints gridBagConstraints = new GridBagConstraints();
      gridBagConstraints.anchor = GridBagConstraints.NORTHWEST;
      gridBagConstraints.insets = new Insets(0, 0, 10, 10);
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.weighty = 1.0;
      gridBagConstraints.ipady = 0;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.gridx = 0;
      centerPanal.add(SelectGenerator, gridBagConstraints);
      
      
    }

    return centerPanal;
  }

  /* This method is used for creating Table area for showing List of Generators.
   *
   */
  private JPanel createMiddlePanel()
  {
    Log.debugLog(className, "createMiddlePanel", "", "", "Method Start");
    JPanel middlePanel = new JPanel(new BorderLayout());
    GridBagLayout gridBagLayout_1 = new GridBagLayout();
    gridBagLayout_1.columnWidths = new int[]{0, 8};
    middlePanel.setLayout(gridBagLayout_1);

    fileDataArea = createTable();
    final JScrollPane pane = new JScrollPane(fileDataArea);
    GridBagConstraints gridBagConstraints_JT = new GridBagConstraints();
    gridBagConstraints_JT.insets = new Insets(0, 0, 0, 0);
    gridBagConstraints_JT.fill = gridBagConstraints_JT.HORIZONTAL;
    gridBagConstraints_JT.ipady = 210;
    gridBagConstraints_JT.gridy = 1;
    gridBagConstraints_JT.gridx = 1;
    gridBagConstraints_JT.weightx = 0.9;
    gridBagConstraints_JT.weighty = 0.9;

    middlePanel.add(pane, gridBagConstraints_JT);
   
    StringBuffer genBuffer = getGenDataFromTool();
    isNewFileDownload = true;
    if(schScheduleData.getBuffer() == null)
      showDataInTable(genBuffer , true);
  
    if(isEditOrAdd)
    {
      if(schScheduleData.getBuffer() != null)
      {
        showDataInTable(schScheduleData.getBuffer() , false);
      }

      String listOfSelectedGenerator = schScheduleData.getListOfSelectGenerator(); // this is used to get List of Selected Generators.
      if(listOfSelectedGenerator != "")
        showSelectedGenerator(listOfSelectedGenerator); // this method is called to show selected generator checked true for the scenario.

    }
    else
    {
      if(schScheduleData.getBufferToEdit() != null)
      {
        showDataInTable(schScheduleData.getBufferToEdit(), false);
        //String str = schScheduleData.getCurGroupData();
        SchGroupData selectedGroupGenerator = schScheduleData.getCurGroupData(); // this is used to get list of Selected Generators for particular group.
        if(selectedGroupGenerator == null)
        {
          if(schScheduleData.getchkUseAllGen() == true) 
          {
            chkDistributeLoad.setSelected(true);
            Object valueTrue = true;
            TableColumn column = creatingTable.getColumnModel().getColumn(0);
            if(!Status.INDETERMINATE.equals(column.getHeaderValue()));
              column.setHeaderValue(Status.SELECTED);
      
            for(int i = 0; i < fileDataArea.getRowCount(); i++)
            {
              fileDataArea.setValueAt(valueTrue, i, 0); // setting value true to the row
            }
          }
          else
            chkDistributeLoad.setSelected(false);
        }
        else
        {
          String idForController = selectedGroupGenerator.getIdForController();
          if(idForController.equals("ALL"))
            chkDistributeLoad.setSelected(true);
          else
          {
            String[] genNameids = idForController.split(",");
            String[] genName = genNameids[0].split(":");
            if(genName.length > 1)
              chkDistributeLoad.setSelected(false);
            else
              chkDistributeLoad.setSelected(true);
          }

          if(selectedGroupGenerator != null && !SchAddGroupGUI.txtGnName.getText().equals(""))
            showSelectedGeneratorForGroup(selectedGroupGenerator); // this method is called to show selected generator checked true for the group.
        }
      }
      else
        showDataInTable(null, false);
    }

    return middlePanel;
  }

  private StringBuffer getGenDataFromTool()
  {
    String tmpArr[][];
    Vector vecCmd = new Vector();
    ServletReport servletReport = new ServletReport();
    String strCmd = "nc_admin";
    String args = "-o show";
    StringBuffer errMsg = new StringBuffer();
    vecCmd = servletReport.runCmd(SchMain.urlCodeBase, "../NetstormServlet", strCmd, args, "", CmdExec.NETSTORM_CMD, schScheduleData.getUserName(), null, errMsg);
    tmpArr = rptUtilsBean.getRecFlds(vecCmd, "", "", "|");   
        
    buffer = new StringBuffer(convert2DArrayToStringBuffer(tmpArr, "|" ).toString());
    return buffer;
  }

  /* this method is used to create lower button panel holding OK,CANCEL and use all generator for all scenario groups checkbox.
   *
   */
  private JPanel createLowerBtnPanel()
  {
    Log.debugLog(className, "createLowerBtnPanel", "", "", "Method Start");
    JPanel temp = new JPanel(new BorderLayout(1, 1));
    JPanel btnPanel = new JPanel();
    temp.add(btnPanel);
    GridBagLayout gridBagLayout = new GridBagLayout();
    gridBagLayout.columnWidths = new int[]{0, 8};
    btnPanel.setLayout(gridBagLayout);
    if(isEditOrAdd == true)
    {
      temp.add(createLowerPanelForControllerAccessSetting(), BorderLayout.NORTH);
      if(schScheduleData.getControllerType().equals("Internal") && isEditOrAdd)
      {
        if(isBothTypeGeneratorSelected())
        {
          chkAccessToExternal.setVisible(true);
          if(schScheduleData.getNetCloudMode() == 2)
            chkAccessToExternal.setSelected(true);
          else
            chkAccessToExternal.setSelected(false);
        }
        else
        {
          chkAccessToExternal.setVisible(false);
        }
      }
    }
    else
    {
      temp.add(new JLabel(" "), BorderLayout.NORTH);
    }

    btnOk = new JButton("OK"); // setting OK button
    GridBagConstraints gridBagConstraints_1 = new GridBagConstraints();
    gridBagConstraints_1.ipady = -5;
    gridBagConstraints_1.fill = GridBagConstraints.BOTH;
    gridBagConstraints_1.insets = new Insets(0, 0, 0, 10);
    btnOk.setActionCommand("ok");
    btnOk.addActionListener(this);
    btnPanel.add(btnOk, gridBagConstraints_1);

    btnCancel = new JButton("Cancel"); // setting CANCEL button
    GridBagConstraints gridBagConstraints = new GridBagConstraints();
    btnCancel.setActionCommand("cancel");
    gridBagConstraints.fill = GridBagConstraints.BOTH;
    btnCancel.setActionCommand("cancel");
    btnCancel.addActionListener(this);
    gridBagConstraints.insets = new Insets(0, 0, 0, 10);
    gridBagConstraints.ipady = -10;
    gridBagConstraints.gridy = 0;
    gridBagConstraints.gridx = 1;
    btnPanel.add(btnCancel, gridBagConstraints);

    temp.add(new JLabel(" "), BorderLayout.SOUTH);
    temp.add(new JLabel(" "), BorderLayout.WEST);
    temp.add(new JLabel(" "), BorderLayout.EAST);

    if(isEditOrAdd == true)
    {
      if(schScheduleData.getchkUseAllGen() == false && schScheduleData.getIsController() == true)
        chkUseAllGen.setSelected(false);
      else
        chkUseAllGen.setSelected(true); // setting state of "use all generator for all scenario groups".
    }
    return temp;
  }

  private JPanel createLowerPanelForControllerAccessSetting()
  {
    Log.debugLog(className, "createLowerPanelForControllerAccessSetting", "", "", "Method Start");
    JPanel lowerPanel = new JPanel();
    GridBagLayout gridBagLayout = new GridBagLayout();
    gridBagLayout.rowHeights = new int[]{0, 7};
    lowerPanel.setLayout(gridBagLayout);
    
    chkUseAllGen = new JCheckBox("Use all Selected Generator(s) for all scenario group(s)"); // setting checkbox for use all generator option.
    chkUseAllGen.setSelected(true);
    GridBagConstraints gridBagConstraints = new GridBagConstraints();
    gridBagConstraints.gridy = 0;
    gridBagConstraints.gridx = 0;
    gridBagConstraints.weightx = 1.0;
    gridBagConstraints.insets = new Insets(0, 0, 0, 0);
    gridBagConstraints.fill = GridBagConstraints.HORIZONTAL;
    gridBagConstraints.anchor = GridBagConstraints.WEST;
    lowerPanel.add(chkUseAllGen, gridBagConstraints);
   
    if(schScheduleData.getControllerType().equals("Internal"))
    {
      chkAccessToExternal = new JCheckBox("Internal controller has connectivity with External controller(" + schScheduleData.getExtControllerInfo() + ")");
      GridBagConstraints gridBagConstraints2 = new GridBagConstraints();
      gridBagConstraints2.gridy = 1;
      gridBagConstraints2.gridx = 0;
      gridBagConstraints2.weightx = 1.0;
      gridBagConstraints2.insets = new Insets(3, 0, 3, 0);
      gridBagConstraints2.fill = GridBagConstraints.HORIZONTAL;
      gridBagConstraints2.anchor = GridBagConstraints.WEST;
      lowerPanel.add(chkAccessToExternal, gridBagConstraints2);
    }
    return lowerPanel;
  }
  
  /* this method is used to create JTable.
  *
  */

  private JTable createTable()
  {
    Log.debugLog(className, "createTable", "", "", "Method Start");
    tableModel = new MyTableModel(arrFiledata, colName);
    creatingTable = new JTable(tableModel)
    {
      public void updateUI()
      {
        super.updateUI();
        TableCellRenderer r = getDefaultRenderer(Boolean.class);
        if(r instanceof JComponent)
        {
          ((JComponent)r).updateUI();
        }
      }

      public Component prepareEditor(TableCellEditor editor, int row, int column) // to give checkBox in Header of JTable.
      {
        Component c = super.prepareEditor(editor, row, column);
        if(c instanceof JCheckBox)
        {
          JCheckBox CheckBox = (JCheckBox)c;
          CheckBox.setBackground(getSelectionBackground());
          CheckBox.setBorderPainted(true);
        }
        return c;
      }

      public Component prepareRenderer(TableCellRenderer renderer, int rowIndex, int vColIndex)//To give alternate color in the table rows.
      {
        try
        {
          Component c = super.prepareRenderer(renderer, rowIndex, vColIndex);
          if(rowIndex % 2 == 0 && !isCellSelected(rowIndex, vColIndex))
            c.setBackground(NSColor.ltableEvenrowcolor());
          if(rowIndex % 2 != 0 && !isCellSelected(rowIndex, vColIndex))
            c.setBackground(NSColor.ltableOddrowcolor());
          return c;
        }
        catch(Exception e)
        {
          Log.stackTraceLog(className, "prepareRenderer", "", "", "Exception", e);
          return null;
        }
      }
    };
    creatingTable.setAutoResizeMode(JTable.AUTO_RESIZE_ALL_COLUMNS);

    int modelColmunIndex = 0; // initialize index for Header Checkbox
    tableModel.addTableModelListener(new HeaderCheckBoxHandler(creatingTable, modelColmunIndex)); // calling Header CheckBox Handler.

    return creatingTable; // returning Table

  }

  /* this method is used to dynamically show data in JTable,
   * buffer of generator file is passed in this method as argument.
   */

  public boolean showDataInTable(StringBuffer sb, boolean isNewFile)
  {
    Log.debugLog(className, "showDataInTable", "", "", "Method Start");

    final int CAVMON_COL_INDEX = 2; //specifing column index of future field
    final int WORK_COL_INDEX = 4; //specifing column index of future field
    // in 3.9.1.release Future column has become Type column which is to show in GUI. it will have either internal or external value
    final int FUTURE_COL_INDEX = 5; //specifing column index of future field
    final int COMMENT_COL_INDEX = 6; //specifing column index of future field
    int TOTAL_COL_NUM = 5; // specifying total no. of fields in first row by "|" separated to show in GUI
    final int NS_GEN_HEADER_ROW = 0; // specifying header rows position in Generator file.
    final int CHECK_COL_INDEX = 0; // used for showing Checkbox for each record at 0 column
    try
    {
      if(!schScheduleData.getControllerType().equals("Internal"))
        TOTAL_COL_NUM = 4;

      if(sb != null)
        arrFiledata = fileDataInTwoDArray(sb); // converting stringbuffer into 2-D array
   
      //Removing rows from table Model
      if(tableModel.getRowCount() > 0)
      {
        for(int i = (tableModel.getRowCount() - 1); i >= 0; i--)
        {
          tableModel.removeRow(i);
        }
      }

      //Removing cols from table Model
      if(tableModel.getColumnCount() > 0)
      {
        for(int i = tableModel.getColumnCount() - 1; i >= 0; i--)
        {
          tableModel.removeColumn(i);
        }
      }
      try
      {
        //set the number of column according to file data.
        if(arrFiledata != null && arrFiledata.length > 0)
        {
          colName = new Object[TOTAL_COL_NUM];
          colName[CHECK_COL_INDEX] = "";
          
	 /* if(arrFiledata.length > 0)
          {
            if(!arrFiledata[NS_GEN_HEADER_ROW][0].equals("GeneratorName")&& !arrFiledata[NS_GEN_HEADER_ROW][1].equals("IP") && !arrFiledata[NS_GEN_HEADER_ROW][3].equals("Location") && !arrFiledata[NS_GEN_HEADER_ROW][5].equals("Type"))
              {
               JOptionPane.showMessageDialog(null, "File format is not correct. Please select appropriate Generator File.", "Error Message", JOptionPane.ERROR_MESSAGE);
               return false;
              }
           }*/

          String colArr[][] = {{"GeneratorName","IP","CavMonAgentPort","Location","Work","Type","Comments"}};  
          for(int i = 0, j = 1; i < colArr[NS_GEN_HEADER_ROW].length; i++)
          {
            if(schScheduleData.getControllerType().equals("Internal"))
            {
              if(i != CAVMON_COL_INDEX && i!= WORK_COL_INDEX && i!= COMMENT_COL_INDEX)
              {
                colName[j++] = colArr[NS_GEN_HEADER_ROW][i];
              }
            }
            else
            {
              if(i != CAVMON_COL_INDEX && i!= WORK_COL_INDEX && i!= COMMENT_COL_INDEX && i!= FUTURE_COL_INDEX)
              {
                colName[j++] = colArr[NS_GEN_HEADER_ROW][i];
              }
            }
          }
      
          for(int ii = 0; ii < colName.length; ii++)
            tableModel.addColumn(colName[ii]);
          if(!isEditOrAdd)
            tableModel.addColumn("Percentage");
          
        }
      }
      catch(Exception excep)
      {
        JOptionPane.showMessageDialog(null, "File format is not correct. Please select appropriate Generator File.", "Error Message", JOptionPane.ERROR_MESSAGE);
        tableModel.fireTableStructureChanged();
        tableModel.fireTableDataChanged();
        return false;  
      }
      try
      {
        if(arrFiledata != null)
        {
          int startRow = 0;
          for(int i = startRow; i < arrFiledata.length; i++)
          {
            Object[] data = new Object[arrFiledata[i].length];
            for(int ii = 0, jj = 0; ii < arrFiledata[i].length; ii++)
            {
              if(ii == CHECK_COL_INDEX)
              {
                if(isNewFile)
                  data[jj++] = new Boolean(true);
                else
                  data[jj++] = new Boolean(false);
              }
              if(schScheduleData.getControllerType().equals("Internal"))
              {
                if(ii!= CAVMON_COL_INDEX && ii!= WORK_COL_INDEX && ii!= COMMENT_COL_INDEX)
                {
                  data[jj++] = arrFiledata[i][ii];
                }
              }
              else
              {
                if(ii!= CAVMON_COL_INDEX && ii!= WORK_COL_INDEX && ii!= COMMENT_COL_INDEX && ii!= FUTURE_COL_INDEX)
                {
                  if(arrFiledata[i][5].toString().toUpperCase().equals("EXTERNAL"))
                  {
                    data[jj++] = arrFiledata[i][ii];
                  }
                }                
              }
            }
            if(schScheduleData.getControllerType().equals("Internal"))
              tableModel.addRow(data);
            else
            {
              if(arrFiledata[i][5].toString().toUpperCase().equals("EXTERNAL"))
                tableModel.addRow(data);
            }
          }

          //if(!isEditOrAdd)
           // updateDistributeLoadPctVal();
          
          if(tableModel.getColumnCount() >= 1)
          {
            creatingTable.getColumnModel().getColumn(0).setHeaderRenderer(new HeaderRenderer(creatingTable.getTableHeader(), 0));
            TableRowSorter<TableModel> sorter = new TableRowSorter<TableModel>(creatingTable.getModel());
            sorter.setSortable(0, false);
            creatingTable.setRowSorter(sorter);
            creatingTable.setModel(creatingTable.getModel());
          }
          setTableProperties(creatingTable);
        }
      }
      catch(Exception exc)
      {
        JOptionPane.showMessageDialog(null, "File format is not correct. Please select appropriate Generator File.", "Error Message", JOptionPane.ERROR_MESSAGE);
        tableModel.fireTableStructureChanged();
        tableModel.fireTableDataChanged();
        return false;  
      }
      return true;
    }
    catch(Exception e)
    {
      if(tableModel.getRowCount() > 0)
      {
        for(int i = (tableModel.getRowCount() - 1); i >= 0; i--)
        {
          tableModel.removeRow(i);
        }
      }

      //Removing cols from table Model
      if(tableModel.getColumnCount() > 0)
      {
        for(int i = tableModel.getColumnCount() - 1; i >= 0; i--)
        {
          tableModel.removeColumn(i);
        }
      }
      JOptionPane.showMessageDialog(null, "File format is not correct. Please select appropriate Generator File.", "Error Message", JOptionPane.ERROR_MESSAGE);
      return false;
    }
  }

  /*this method is to convert string 2D array into StringBuffer*/
  private StringBuffer convert2DArrayToStringBuffer(String[][] arr , String seperator)
  {
    StringBuffer results = new StringBuffer();
    String separator = seperator;
    String values [][] = arr;

    for (int i = 0; i < values.length; ++i)
    {
      for (int j = 0; j < values[i].length; j++)
      {
        if(j == (values[i].length - 1))
          results.append(values[i][j]).append("\n");
        else
          results.append(values[i][j]).append(separator);
      }
    }

    return results;
  }
 
  /*
   * this method is used to convert stringbuffer in 2-D array.
   */

  public String[][] fileDataInTwoDArray(StringBuffer strBuff)
  {
    Log.debugLog(className, "fileDataInTwoDArray", "", "", "Method Start");
    String[][] arrFlds = null;
    String[] arrDataFile = strBuff.toString().split("\n");
    if(arrDataFile.length > 0)
    {
      //getting first index length
      String[] firstRowData = rptUtilsBean.split(arrDataFile[0], "|");
      arrFlds = new String[arrDataFile.length][firstRowData.length];
      //Start loop to traverse no. of line

      for(int k = 0; k < arrDataFile.length; k++)
      {
        //storing no. of values splited by delimiter
        String[] arrTemp = rptUtilsBean.split(arrDataFile[k], "|");
        if(arrTemp.length > arrFlds[k].length)
        {
          String arrNewTemp[][] = new String[arrDataFile.length][arrTemp.length];
          arrFlds = copy2DArray(arrFlds, arrNewTemp);
        }
        arrFlds[k] = arrTemp;
      }
    }

    return arrFlds;
  }

  /* this method is used to replace old data with the new data as file is changed.
   *
   */

  public String[][] copy2DArray(String[][] arrOldTemp, String[][] arrNewTemp)
  {
    Log.debugLog(className, "copy2DArray", "", "", "Method Start");
    try
    {
      System.arraycopy(arrOldTemp, 0, arrNewTemp, 0, arrOldTemp.length);
      return arrNewTemp;
    }
    catch(Exception ex)
    {
      Log.stackTraceLog(className, "copy2DArray", "", "", "Exception in copy2DArray()", ex);
      return null;
    }
  }

  /* this method is used to show state of Selected Generators for the scenario checked i.e true.
  *
  */
  public void showSelectedGenerator(String getListOfSelectedGenerator)
  {
    Log.debugLog(className, "showSelectedGenerator", "", "", "Method Start");
    String list = getListOfSelectedGenerator;

    String[] SplitNS_Gen_String = list.split("\\|");
    // spliting String of selected generator by "|" separator.
    Object valueTrue = true;
    for(int i = 0; i < fileDataArea.getRowCount(); i++)
    {
      for(int index = 0; index < SplitNS_Gen_String.length; index++)
      {
        if(SplitNS_Gen_String[index].equals(fileDataArea.getValueAt(i, 1)))
        {
          fileDataArea.setValueAt(valueTrue, i, 0); // setting value true to the row
        }
      }
    }
  }

  /* this method handle all the events i.e OK and CANCEL
   *
   */

  public void actionPerformed(ActionEvent e)
  {
    Log.debugLog(className, "actionPerformed", "", "", "Method Start");
    String command = e.getActionCommand();
    Object source = e.getSource();

    if(isEditOrAdd)
    {
      
      if(source instanceof JButton)
      {
        boolean chkUseAllPreviousPosition; // this variable is used to hold the last value of use all generator checkbox
        boolean ChkUseAllNewPosition; // this value is used to hold the new value of use all generator checkbox
        if(command.equals("ok")) //if ok button is clicked
        {
          if(validateGeneratorData()) // calling function to validate GeneratorData
          {
            if(schScheduleData.getControllerType().equals("Internal"))
            {
              if(isBothTypeGeneratorSelected())
              {
                if(!chkAccessToExternal.isSelected())
                {
                  JOptionPane.showMessageDialog(null, "You have selected both(Internal & External) Generator(s) to use but didn't provide access to External Controller.\r\nEither use only Internal/External Generator(s) or provide access to External Controller.", "Error Message", JOptionPane.ERROR_MESSAGE);
                  return;
                }
                else
                {
                  schScheduleData.setNetCloudMode(2);
                }
              }
              else
              {
                schScheduleData.setNetCloudMode(0);
              }
            }
            int DialogOption = JOptionPane.showConfirmDialog(null, "Generator(s) applied successfully.", "Information Message" , JOptionPane.OK_CANCEL_OPTION , JOptionPane.INFORMATION_MESSAGE);
            if(DialogOption == JOptionPane.OK_OPTION)
            {
              // setting up netcloud mode
              if(schScheduleData.getControllerType().equals("Internal"))
              {
                // code for checkbox
              }
              schScheduleData.setIsController(true); // setting controller mode true
              chkUseAllPreviousPosition = schScheduleData.getchkUseAllGen(); // getting last position of use all checkbox
              schScheduleData.setchkUseAllGen(chkUseAllGen.isSelected());
              ChkUseAllNewPosition = schScheduleData.getchkUseAllGen(); // getting new position of use all checkbox
              if(buffer != null)
              {
                schScheduleData.setFilePath(StrGenFilePath);
                schScheduleData.setBuffer(buffer);
              }
              else
              {
                schScheduleData.setFilePath(schScheduleData.getFilePath());
                schScheduleData.setBuffer(schScheduleData.getBuffer());
              }

              schScheduleData.setListOfSelectGenerator(ListOfSelectedGeneratorIs);
              setDataToEditGenerator(schScheduleData.getBuffer()); // calling function to show updated Generator List to EditGeneratorGUI.
              setGeneratorsToGroups(schScheduleData.getListOfGroupData() , chkUseAllPreviousPosition , ChkUseAllNewPosition); //calling function to set value of each group under Generator column.
              schScenGroup.resetGroupTableForGenerator(util, schScheduleData); // redrawing group table.
              dialog.dispose();
            }
            else
              return;
          }
          else // use Generator is not selected then default value is set in SchScheduleData
          {
            ArrayList GroupNamesList;
            int dialogOption = JOptionPane.showConfirmDialog (null, "<html> No Generator(s) is selected. So load Generator(s) feature will not be applicable <br> and you will loose all Generator(s) configuration.</html>" , "Warning", JOptionPane.YES_NO_OPTION );
            if(dialogOption == JOptionPane.YES_OPTION)
            {
              schScheduleData.setNetCloudMode(0); // default netcloud mode
              schScheduleData.setIsController(false); // setting controller mode false
              schScheduleData.setBuffer(null); // for setting buffer in case when no Generator is applied in the scenario.
              schScheduleData.setFilePath("");
              schScheduleData.setchkUseAllGen(false);
              schScenGroup.resetGroupTableForGenerator(util, schScheduleData); // redrawing group table.
              GroupNamesList = new ArrayList();
              GroupNamesList = schScheduleData.getListOfGroupData();
              String DefaultidForController = "NA";
              double numOrPct = 0.0;
              for(int i = 0; i < GroupNamesList.size(); i++)
              {
                SchGroupData gpData = (SchGroupData)GroupNamesList.get(i); // getting data of each group in the scenario.
                if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
                	numOrPct = gpData.getPctUser();
                else
                	numOrPct = gpData.getNumVuser();
                util.addOrEditGroup(schScheduleData, gpData.getGPName(), DefaultidForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
              }
              dialog.dispose();
            }
            else if(dialogOption == JOptionPane.NO_OPTION)
              return;
          }
        }
        else if(command.equals("cancel")) // event handling for Cancel button
        {
          dialog.dispose();
        }
 
        else if(command.equals("browse")) // event handling for Browse button to Browse GeneratorFile for the scenario.
        {
          String path = "/";
          try
          {
            //calling FileAndDirChooser to select generator file from server
            FileAndDirChooser fileAndDirChooser = new FileAndDirChooser(this, path, "F"); 
          }
          catch(Exception excep)
          {
            JOptionPane.showMessageDialog(null, "Connection to the server could not be initialized successfully. Please try again...", "Error Message", JOptionPane.ERROR_MESSAGE);
            return;
          }
        }
      }
    }
    else
    {
      if(source instanceof JButton)
      {
        if(command.equals("ok")) // handling OK button event
        { 
          if(schScheduleData.getchkUseAllGen())
            if(!checkAllGenIsSelectedForALL())
              return;
          
          int colIndexToview = creatingTable.convertColumnIndexToView(0);
          TableColumn column = creatingTable.getColumnModel().getColumn(colIndexToview);
          if(Status.SELECTED.equals(column.getHeaderValue()))
          {
            int colIndxForDistPct = tableModel.getColumnCount() - 1;
            double pctIndexForDist = 100;
            if(fileDataArea.getRowCount() > 1)
              pctIndexForDist = 100 / (fileDataArea.getRowCount() -1);

            boolean isALLDist = true;
            try
            {
              for(int i = 0; i < fileDataArea.getRowCount(); i++)
              {
        	if(pctIndexForDist != Double.parseDouble(fileDataArea.getValueAt(i, colIndxForDistPct).toString()));
        	{
        	  isALLDist = false;
        	  break;
        	}
              }
            }
            catch(Exception ex)
            {
              JOptionPane.showMessageDialog(null, "Percentage should be in Numeric.", "ERROR", JOptionPane.ERROR_MESSAGE);
              return;
            }
            
            if(isALLDist || chkDistributeLoad.isSelected() == true)
              SchAddGroupGUI.txtGnName.setText("ALL");
            else if(!GetSelectedGenerator())
              return;
           
              dialog.dispose();
          }
          else if(GetSelectedGenerator())
          {
            dialog.dispose();
          }
          else
          {
            return;
          }
        }
        else if(command.equals("cancel")) // handling CANCEL event.
        {
          dialog.dispose();
        }
      }
    }
  }
  
  /*
   * Method is used to check Whether All Genetors are selected of not. Select ALL generator Option is Selected in this case. 
   */
  
  public boolean checkAllGenIsSelectedForALL()
  {
    Log.debugLog(className, "checkAllGenIsSelectedForALL", "", "", "Method Start");
    int ctr = 0;
    for(int i=0; i < tableModel.getRowCount(); i++)
      if(tableModel.getValueAt(i, 0).equals(true))
        ctr++;
    
    if(ctr != tableModel.getRowCount())
    {
      JOptionPane.showMessageDialog(null, "<html><p>'Use All Selected Generator(s) for all scenario group(s)' option is selected.</p> <p> Please select All Generator(s) to apply. </p></html>", "ERROR", JOptionPane.ERROR_MESSAGE);
      return false;
    }
    return true;
  }
  
  public boolean GetSelectedGenerator()
  {
    Log.debugLog(className, "GetSelectedGenerator", "", "", "Method Start");
    int count = 0;
    String delimeter = ","; // to form string of selected Generator by comma separated to be passed to the Group.
    ArrayList SelectedGeneratorName = new ArrayList(); // this arraylist hold the list of selected generator for the whole scenario,
    // and only those generators are shown here in this GUI,
    String GeneratorName = "";

    double toatlDistribution = 0;
    int pctDestColIndex = tableModel.getColumnCount() - 1;
    try
    {
      for(int i = 0; i < fileDataArea.getRowCount(); i++)
      {
	Object checkBox = fileDataArea.getValueAt(i, 0);
	boolean isChecked = ((Boolean)checkBox).booleanValue();
	if(isChecked)
	{
	  count++;
	  if(chkDistributeLoad.isSelected() == true)
	    SelectedGeneratorName.add(fileDataArea.getValueAt(i, 1).toString());
	  else
	    SelectedGeneratorName.add(fileDataArea.getValueAt(i, 1).toString() + ":" + fileDataArea.getValueAt(i, pctDestColIndex).toString());
	  toatlDistribution = toatlDistribution + Double.parseDouble(fileDataArea.getValueAt(i, pctDestColIndex).toString());
	}
      }
    }
    catch(Exception ex)
    {
      JOptionPane.showMessageDialog(null, "Percentage should be in Numeric.", "ERROR", JOptionPane.ERROR_MESSAGE);
      return false;
    }

    if(!chkDistributeLoad.isSelected() && toatlDistribution != 100)
    {
      JOptionPane.showMessageDialog(null, "Total Percent Distribution of selected Generator should be 100%.", "ERROR", JOptionPane.ERROR_MESSAGE);
      return false;
    }
    
    for(Iterator<String> it = SelectedGeneratorName.iterator(); it.hasNext();)
    {
      GeneratorName += it.next();
      if(it.hasNext())
      {
        GeneratorName += delimeter;
      }
    }
    if(count < 1)
    {
      JOptionPane.showMessageDialog(null, "Please select one of the available Generator(s) from the list.", "Error Message", JOptionPane.ERROR_MESSAGE);
      return false;
    }
    else
    {
      SchAddGroupGUI.txtGnName.setText(GeneratorName);
      return true;
    }
  }

  /*  this method is used to set Generators either ALL or NA for all the groups available in the scenario.
   *
   */

  public void setGeneratorsToGroups(ArrayList<Object> ListOfGroupNames , boolean chkUseAllPreviousPos , boolean ChkUseAllNewPos)
  {
    Log.debugLog(className, "setGeneratorsToGroups", "", "", "Method Start");
    ArrayList<String> StrGenName = new ArrayList<String>();
    ArrayList<String> StrGrpName = new ArrayList<String>();
    String idForController = "";
    double numOrPct = 0.0;
    if(chkUseAllPreviousPos == false && ChkUseAllNewPos == true)
    {
      idForController = "ALL";
      for(int i = 0; i < ListOfGroupNames.size(); i++)
      {
        SchGroupData gpData = (SchGroupData)ListOfGroupNames.get(i); // getting data of each group in the scenario.
        if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
          numOrPct = gpData.getPctUser();
        else
          numOrPct = gpData.getNumVuser();
        util.addOrEditGroup(schScheduleData, gpData.getGPName(), idForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
      }
    }
    else if(chkUseAllPreviousPos == true && ChkUseAllNewPos == false)
    {
      idForController = "NA";
      for(int i = 0; i < ListOfGroupNames.size(); i++)
      {
        SchGroupData gpData = (SchGroupData)ListOfGroupNames.get(i); // getting data of each group in the scenario.
        if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
          numOrPct = gpData.getPctUser();
        else
          numOrPct = gpData.getNumVuser();
        util.addOrEditGroup(schScheduleData, gpData.getGPName(), idForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
      }
    }
    else if(chkUseAllPreviousPos == true && ChkUseAllNewPos == true)
    {
      for(int i = 0; i < ListOfGroupNames.size(); i++)
      {
        SchGroupData gpData = (SchGroupData)ListOfGroupNames.get(i); // getting data of each group in the scenario.
        idForController = gpData.getIdForController().trim();
        if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
          numOrPct = gpData.getPctUser();
        else
          numOrPct = gpData.getNumVuser();
        util.addOrEditGroup(schScheduleData, gpData.getGPName(), idForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
      }   
    }
    else if(chkUseAllPreviousPos == false && ChkUseAllNewPos == false)
    {
      HashMap hMap = new HashMap();
      if(!ListOfSelectedGeneratorIs.equals(""))
        ArrNewSelectedGenerator = ListOfSelectedGeneratorIs.split("\\|");
      
      for(int i = 0; i < ListOfGroupNames.size(); i++)
      {
        SchGroupData gpData = (SchGroupData)ListOfGroupNames.get(i); 
        idForController = gpData.getIdForController();
       if(!(idForController.equals("NA")) && !(idForController.equals("ALL")))
        {
          String[] GrpGenerator;
          if(idForController.contains(","))
            GrpGenerator = idForController.split(",");
          else
          {
            GrpGenerator = new String[1];
            GrpGenerator[0] = idForController;
          }
        
          String ModifiedIdForController = "";
          for(int ii = 0; ii < ArrNewSelectedGenerator.length; ii ++)
          {
            for(int jj = 0; jj < GrpGenerator.length; jj ++)
            {
              if(GrpGenerator[jj].trim().equals(ArrNewSelectedGenerator[ii]))
                ModifiedIdForController += GrpGenerator[jj] + ",";
            }
          }
          if(!ModifiedIdForController.equals("")) 
          {
            ModifiedIdForController = ModifiedIdForController.substring(0, ModifiedIdForController.lastIndexOf(","));
            hMap.put(gpData.getGPName(), ModifiedIdForController);
          
            String[] GrpModifiedGenerator;
            if(ModifiedIdForController.contains(","))
              GrpModifiedGenerator = ModifiedIdForController.split(",");
            else
            {
              GrpModifiedGenerator = new String[1];
              GrpModifiedGenerator[0] = ModifiedIdForController;
            }  
          
            ArrayList<String> modifiedListValues = new ArrayList<String>();
            for(int index = 0 ; index < GrpModifiedGenerator.length; index ++)
              modifiedListValues.add(GrpModifiedGenerator[index]);
              
            for(int indexOfModified = 0; indexOfModified < GrpGenerator.length; indexOfModified ++)
            {
              if(!modifiedListValues.contains(GrpGenerator[indexOfModified]))
              {
                if(!StrGenName.contains(GrpGenerator[indexOfModified]))
                  StrGenName.add(GrpGenerator[indexOfModified]);
                if(!StrGrpName.contains(gpData.getGPName()))
                  StrGrpName.add(gpData.getGPName());
              }
            } 
          }
          else
          {
            hMap.put(gpData.getGPName(), "NA");
            if(!StrGenName.contains(GrpGenerator[0]))
            {
              for(int indexIs = 0; indexIs < GrpGenerator.length; indexIs ++)
                StrGenName.add(GrpGenerator[indexIs]);
            }
            if(!StrGrpName.contains(gpData.getGPName()))
              StrGrpName.add(gpData.getGPName()); 
          }
        }
      }

      if(StrGenName.size() != 0 && StrGrpName != null)
      {
        String GeneratorNotSelected = "";
        String GeneratorUsedInGroup = "";
        for(Iterator<String> it = StrGenName.iterator(); it.hasNext();)
        {
          GeneratorNotSelected += it.next();
          if(it.hasNext())
            GeneratorNotSelected += ",";
        }
        for(Iterator<String> itr = StrGrpName.iterator(); itr.hasNext();)
        {
          GeneratorUsedInGroup += itr.next();
          if(itr.hasNext())
            GeneratorUsedInGroup += ",";
        }
        int ApproveOption = JOptionPane.showConfirmDialog(null, "<html>Generator(s) " + GeneratorNotSelected + " is not selected but still used by the Group(s) " + GeneratorUsedInGroup + ".<br> It will remove " + GeneratorNotSelected + " Generator(s) from " + GeneratorUsedInGroup + " Group(s).<br> Do you agree with this change?</html>", "Information Message" , JOptionPane.YES_NO_OPTION , JOptionPane.QUESTION_MESSAGE);
        if(ApproveOption == JOptionPane.YES_OPTION)
        {
          for(int i = 0; i < ListOfGroupNames.size(); i++)
          {
            SchGroupData gpData = (SchGroupData)ListOfGroupNames.get(i); // getting data of each group in the scenario.
            idForController = gpData.getIdForController();
                
            if(idForController.equals("ALL") || idForController.equals("NA"))
            {
              if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
                numOrPct = gpData.getPctUser();
              else
                numOrPct = gpData.getNumVuser();
              util.addOrEditGroup(schScheduleData, gpData.getGPName(), idForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
            }
            else
            {
              Set set = hMap.entrySet();
              Iterator iter = set.iterator();
              while(iter.hasNext()) 
              {
                Map.Entry me = (Map.Entry)iter.next();
                if(me.getKey().equals(gpData.getGPName()))
                {
                  idForController =  (String)me.getValue();
                  if(schScheduleData.getDistributionMode() == SchScheduleData.PCT_MODE)
                    numOrPct = gpData.getPctUser();
                  else
                    numOrPct = gpData.getNumVuser();
                  util.addOrEditGroup(schScheduleData, gpData.getGPName(), idForController, gpData.getSType(), gpData.getUserProfile(), gpData.getType(), gpData.getScript(), gpData.getUrl(), numOrPct, gpData.getId(), gpData.getRowIndex(), false, true, gpData);
                }   
              }
            }    
          }
        }
        else
        {
          String resetGen = "";
          StringBuffer resetBuffer = new StringBuffer();
          if(GeneratorNotSelected.contains(","))
            resetGen = GeneratorNotSelected.replace(",", "|");
          else
            resetGen = GeneratorNotSelected;
          
          String UpdatedListOfSelectedGenerator = schScheduleData.getListOfSelectGenerator(); // this is used to get List of Selected Generators.
          UpdatedListOfSelectedGenerator = UpdatedListOfSelectedGenerator + "|" + resetGen;
          schScheduleData.setListOfSelectGenerator(UpdatedListOfSelectedGenerator);
          resetBuffer = schScheduleData.getBuffer();
          String StrEditDataUpdated = resetBuffer.toString();
          String[] editArrUPdate = StrEditDataUpdated.split("\n");
          String[] editArrForSelectedGeneratorsupdated = schScheduleData.getListOfSelectGenerator().split("\\|");
          StringBuffer editBufferupdated = new StringBuffer();

          editBufferupdated.append(editArrUPdate[0]); // for HeaderLine
          editBufferupdated.append("\n");

          for(int j = 1; j < editArrUPdate.length; j++) // traversing all rows and storing all that row data which is checked true.
          {
            for(int jj = 0; jj< editArrForSelectedGeneratorsupdated.length; jj++)
            {
             if(editArrUPdate[j].substring(0, editArrUPdate[j].indexOf("|")).equals(editArrForSelectedGeneratorsupdated[jj]))
              {
                editBufferupdated.append(editArrUPdate[j]);
                editBufferupdated.append("\n");
              }
            }
          }
          schScheduleData.setBufferToEdit(editBufferupdated);
        }
      }
    } 
  }
  
/* this method is used to show state of Selected Generators for the scenario checked i.e true.
  *
  */

  public void showSelectedGeneratorForGroup(SchGroupData selectedGroupGenerator)
  {
    Log.debugLog(className, "showSelectedGenerator", "", "", "Method Start");
    String selectedGeneratorForGroup = selectedGroupGenerator.getIdForController();

    Object valueTrue = true;
    int colIndxForDistPct = tableModel.getColumnCount() - 1;
    
    if(selectedGeneratorForGroup.equals("ALL"))
    {
      TableColumn column = creatingTable.getColumnModel().getColumn(0);
      if(!Status.INDETERMINATE.equals(column.getHeaderValue()));
        column.setHeaderValue(Status.SELECTED);
      for(int i = 0; i < fileDataArea.getRowCount(); i++)
      {
        fileDataArea.setValueAt(valueTrue, i, 0); // setting value true to the row
      }
    }
    if(!selectedGeneratorForGroup.equals("ALL") && !selectedGeneratorForGroup.equals("NA"))
    {
      String[] strArrGeneratorForGroup = selectedGeneratorForGroup.split(",");
      Log.debugLog(className, "showSelectedGenerator", "", "", "1234 selectedGeneratorForGroup= " + selectedGeneratorForGroup);
      // spliting String of selected generator by "," separator.
      for(int i = 0; i < fileDataArea.getRowCount(); i++)
      {
	fileDataArea.setValueAt("0", i, colIndxForDistPct);
        for(int index = 0; index < strArrGeneratorForGroup.length; index++)
        {
          String strGenName = strArrGeneratorForGroup[index];
          String arrGenLoadTemp[] = strGenName.split(":");
          if(arrGenLoadTemp.length > 1)
            strGenName = arrGenLoadTemp[0];
          if(strGenName.equals(fileDataArea.getValueAt(i, 1)))
          {
            fileDataArea.setValueAt(valueTrue, i, 0); // setting value true to the row
        
            if(arrGenLoadTemp.length > 1)
            {
              fileDataArea.setValueAt(arrGenLoadTemp[1], i, colIndxForDistPct);
            }
            else
              fileDataArea.setValueAt("0", i, colIndxForDistPct);
          }
        }
      }
    }
    //isDistributedLoadEqually();
  }

 /* private void isDistributedLoadEqually()
  {
    int colIndxForDistPct = tableModel.getColumnCount() - 1;
    double pctIndexForDist = (double)100 / (fileDataArea.getRowCount() -1);
    boolean isALLDist = true;
    for(int i = 0; i < fileDataArea.getRowCount(); i++ )
    {
      if(pctIndexForDist != Double.parseDouble(fileDataArea.getValueAt(i, colIndxForDistPct).toString()));
      {
	isALLDist = false;
	break;
      }
    }
    
    if(isALLDist)
      chkDistributeLoad.setSelected(true);
    else
      chkDistributeLoad.setSelected(false);
  }*/

  /*this method is used to open directory of given path of netstorm machine, form where we have to upload Generator File.
   *
   */

  public boolean fileUploadFromServer(String FilePathOnServer)
  {
    Log.debugLog(className, "fileUploadFromServer", "", "", "Method Start");
    StringBuffer StrBuffer = new StringBuffer(); // initializing StringBuffer to hold data in buffer of Generator File
    String fileName = ""; // this variable is used to get file name from the Generator Text field containg file path for Generator File
    fileName = txtGnName.getText();
    StringBuffer errMsg = new StringBuffer();
    ServletReport servletReportObj = new ServletReport();

    if(!fileName.equals(""))
    {
      try
      {
        StrBuffer = servletReportObj.readFile(schMain.urlCodeBase, "../NetstormServlet", fileName, errMsg);
        if(StrBuffer == null)
        {
          JOptionPane.showMessageDialog(null, "File format is not correct. Please select appropriate Generator File.", "Error Message", JOptionPane.ERROR_MESSAGE);
          txtGnName.setText("Please Browse a Generator(s) File.");
          file = null;
          return false; 
        }
        else
        {
          //StrBuffer = new StringBuffer("");
          buffer = new StringBuffer(StrBuffer.toString());
          if(!showDataInTable(buffer, true))
          {
            txtGnName.setText("Please Browse a Generator(s) File.");
            file = null;
            tableModel.fireTableStructureChanged();
            tableModel.fireTableDataChanged();
            return false;
          }

          file = new File(fileName);

          StrGenFilePath = file.getPath();
          if(schScheduleData.getControllerType().equals("Internal") && isEditOrAdd)
          {
            if(isBothTypeGeneratorSelected())
            {
              chkAccessToExternal.setVisible(true);
              if(schScheduleData.getNetCloudMode() == 2)
                chkAccessToExternal.setSelected(true);
              else
                chkAccessToExternal.setSelected(false);
            }
            else
            {
              chkAccessToExternal.setVisible(false);
            }
          }
        }
      }
      catch(Exception e)
      {
        JOptionPane.showMessageDialog(null, "Connection to the server is not initialized or selected file is not in correct format.", "Error Message", JOptionPane.ERROR_MESSAGE);
        return false;
      }
    }
    return true;
  }
   

  /** This Method is used to check whether user has selected at least one external generator or not.
   *  in case when user is using netCloud Mode 2 which is 'With Access to External'
   *  @return
   */
  public boolean isBothTypeGeneratorSelected()
  {
    Log.debugLog(className, "isBothTypeGeneratorSelected", "", "", "Method Start");
    boolean isExternalGenSelected = false;
    boolean isInternalGenSelected = false;
    boolean result = false;
    try
    {
      //if(file != null)
     // {
        for(int i = 0; i < fileDataArea.getRowCount(); i++)
        {
          Object checkBox = fileDataArea.getValueAt(i, 0);
          boolean isChecked = ((Boolean)checkBox).booleanValue();
          
          if(isChecked)
          {
            if(fileDataArea.getValueAt(i, 4).toString().toUpperCase().equals("EXTERNAL"))
            {
              isExternalGenSelected = true;
            }
            else if(fileDataArea.getValueAt(i, 4).toString().toUpperCase().equals("INTERNAL"))
            {
              isInternalGenSelected = true;
            }
            else
            {
            }
          }          
        }
        if(isInternalGenSelected && isExternalGenSelected)
        {
          result = true;
          return result;
        }
        else
          return result;
     // }
     // else
       // return result;
    }
    catch(Exception e)
    {
      return result;
    }
  }
  
  /*
   * this method is used to validate whether One of the Generator is selected out of showing Generator or not.
   */

  private boolean validateGeneratorData()
  {
    Log.debugLog(className, "validateGeneratorData", "", "", "Method Start");
    int count = 0;
   // if(file != null)
   // {
      for(int i = 0; i < fileDataArea.getRowCount(); i++)
      {
        Object checkBox = fileDataArea.getValueAt(i, 0);
        boolean isChecked = ((Boolean)checkBox).booleanValue();
        if(isChecked)
        {
          count++;
          ListOfSelectedGeneratorIs += fileDataArea.getValueAt(i, 1).toString() + "|";
        }
      }
      if(!ListOfSelectedGeneratorIs.equals(""))
        ListOfSelectedGeneratorIs = ListOfSelectedGeneratorIs.substring(0, ListOfSelectedGeneratorIs.lastIndexOf("|"));
      
      if(count >= 1)
      {
        return true;
      }
      else
      {
        Log.errorLog(className, "validateGeneratorData", "", "", "No Generator is set selected" + "");
        return false;
      }
   // }
   /* else
    {
      Log.errorLog(className, "validateGeneratorData", "", "", "No Generator(s) file is selected" + "");
      JOptionPane.showMessageDialog(null, "No Generator(s) file is selected. Please Browse a Generator(s) file.", "ERROR", JOptionPane.ERROR_MESSAGE);
      return false;
    }*/
  }

  /* this method is used to pass buffer of selected Generator for the whole scenario to the EditGeneratorGUI window to edit for each particular group.
   *
   */

  public void setDataToEditGenerator(StringBuffer StrngBuffForEdit)
  {
    Log.debugLog(className, "setDataToEditGenerator", "", "", "Method Start");
    String StrEditData = StrngBuffForEdit.toString();
    String[] editArr = StrEditData.split("\n");
    String[] editArrForSelectedGenerators = ListOfSelectedGeneratorIs.split("\\|");
    StringBuffer editBuffer = new StringBuffer();

    /*now, we dont need header line to show in generator table.*/
    //editBuffer.append(editArr[0]); // for HeaderLine
    //editBuffer.append("\n");
    for(int j = 1; j < editArr.length; j++) // traversing all rows and storing all that row data which is checked true.
    {
      for(int jj = 0; jj< editArrForSelectedGenerators.length; jj++)
      {
        if(editArr[j].substring(0, editArr[j].indexOf("|")).equals(editArrForSelectedGenerators[jj]))
        {
          editBuffer.append(editArr[j]);
          editBuffer.append("\n");
        }
      }
    }

    schScheduleData.setBufferToEdit(editBuffer);
  }

  // creating class MyTableModel holding methods of DefaultTableModel
  class MyTableModel extends DefaultTableModel
  {
    private Object[] columnName = null;
    private Object[][] data = null;

    public MyTableModel(Object[][] data1, Object[] columnName1)
    {
      super(data1, columnName1);
      data = data1;
      columnName = columnName1;
    }

    public void removeColumn(int column) // method to remove column
    {
      columnIdentifiers.remove(column);
    }

    public Class getColumnClass(int c) // method to get Class at 0 column
    {
      switch(c)
      {
        case 0: return Boolean.class;
        case 1: return String.class;
        case 2: return Integer.class;
        case 3: return String.class;
        default: return Object.class;
      }
    }

    public boolean isCellEditable(int nRow, int nCol) // method to provide editable functionality to the cell at 0 column
    {
      int destPctColIndex = tableModel.getColumnCount() - 1;
      if(nCol == 0)
        return true;
      else if(nCol == destPctColIndex && chkDistributeLoad.isSelected() == false)
      {
        return true;
      }
      else
        return false;
    }
    
  }

  private JTable setTableProperties(JTable table)
  {
    Log.debugLog(className, "setTableProp", "", "", "Start method");
    JTableHeader tableHeader = table.getTableHeader();
    table.setBackground(NSColor.leftPanelcolor());
    table.setFont(NSColor.mediumPlainFont());
    table.setGridColor(NSColor.rightPanelcolor());
    table.setSelectionForeground(Color.white);
    table.setBorder(BorderFactory.createLineBorder(NSColor.tableHeaderFgColor()));
    tableHeader.setOpaque(false);
    tableHeader.setBackground(NSColor.tableHeaderColor());
    tableHeader.setForeground(NSColor.tableHeaderFgColor());
    tableHeader.setPreferredSize(new Dimension(100, 20));
     
    tableHeader.setReorderingAllowed(false);
    table.setTableHeader(tableHeader);
    TableColumn tc = null;

    for(int j = 0; j < table.getColumnCount(); j++)
    {
      tc = table.getColumnModel().getColumn(j);
      tc.setCellRenderer(new ColumnRenderer());

      if(j == 0)
      {
        tc.setMaxWidth(25);
        tc.setMinWidth(25);
      }
      else
        tc.setPreferredWidth(200);
    }
    return table;
  }

  class ColumnRenderer extends DefaultTableCellRenderer
  {
    JCheckBox checkBox = new JCheckBox();

    public ColumnRenderer()
    {
      super();
    }

    public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
    {
      Component cell = super.getTableCellRendererComponent(table, value, isSelected, hasFocus, row, column);

      JLabel currentCell = (JLabel)cell;
      currentCell.setHorizontalAlignment(JLabel.CENTER);
      currentCell.setToolTipText(currentCell.getText());
      if(value instanceof Boolean)
      {
        boolean bool = ((Boolean)value).booleanValue();
        checkBox.setSelected(bool);
        if(!isEditOrAdd)
          updateDistributeLoadPctVal(); // auto distributing pct value between selected generators
        checkBox.setHorizontalAlignment(JLabel.CENTER);
        if(schScheduleData.getControllerType().equals("Internal") && isEditOrAdd)
        {
          if(isBothTypeGeneratorSelected())
          {
            chkAccessToExternal.setVisible(true);
            chkAccessToExternal.setSelected(true);
          }
          else
            chkAccessToExternal.setVisible(false);
        }
        return checkBox;
      }
      else
      {
        boolean checkSelected = ((Boolean)table.getValueAt(row, 0)).booleanValue();
        return cell;
      }
    }
  }


  class HeaderRenderer extends JCheckBox implements TableCellRenderer
  {
    private int targetColumnIndex;
    public HeaderRenderer(JTableHeader header, int index)
    {
      super((String)null);
      this.targetColumnIndex = index;
      setOpaque(false);
      setFont(header.getFont());
      header.addMouseListener(new MouseAdapter()
      {
        public void mouseClicked(MouseEvent e)
        {
          JTableHeader header = (JTableHeader)e.getSource();
          JTable table = header.getTable();
          TableColumnModel columnModel = table.getColumnModel();
          int vci = columnModel.getColumnIndexAtX(e.getX());
          int mci = table.convertColumnIndexToModel(vci);

          if(mci == targetColumnIndex)
          {
            TableColumn column = columnModel.getColumn(vci);
            Object headerValue = column.getHeaderValue();
            boolean b = Status.DESELECTED.equals(headerValue)?true:false;
            TableModel model = table.getModel();
            for(int i=0; i<model.getRowCount(); i++) model.setValueAt(b, i, mci);
              column.setHeaderValue(b?Status.SELECTED:Status.DESELECTED);
            
            if(schScheduleData.getControllerType().equals("Internal") && isEditOrAdd)
            {
              if(isBothTypeGeneratorSelected())
              {
                chkAccessToExternal.setVisible(true);
                chkAccessToExternal.setSelected(true);
              }
              else
                chkAccessToExternal.setVisible(false);                
            }
          }
        }
      });
    }
    public Component getTableCellRendererComponent(JTable tbl, Object val, boolean isS, boolean hasF, int row, int col)
    {
      TableCellRenderer r = tbl.getTableHeader().getDefaultRenderer();
      JLabel label =(JLabel)r.getTableCellRendererComponent(tbl, val, isS, hasF, row, col);
      if(targetColumnIndex==tbl.convertColumnIndexToModel(col))
      {
        if(val instanceof Status)
        {
          switch((Status)val)
          {
            case SELECTED:
              setSelected(true);
              setEnabled(true);
              break;
            case DESELECTED:
              setSelected(false);
              setEnabled(true);
              break;
            case INDETERMINATE:
              setSelected(false);
              setEnabled(true);
              break;
          }
        }
        else
        {
          if(isEditOrAdd && isNewFileDownload)
            setSelected(true);
          else
            setSelected(false); 
            setEnabled(true);
        }
        label.setIcon(new ComponentIcon(this));
        label.setText(null);
        label.setHorizontalAlignment(SwingConstants.CENTER); // setting Header Checkbox to center
      }
      return label;
    }
  }
  
  /*
   * This Method is used to equally Distribute Load pct value between generators
   * @param null 
   */
  private void updateDistributeLoadPctVal()
  {
    int totalRowInTable = tableModel.getRowCount();
    int pctDistColIndex = tableModel.getColumnCount() -1;
    int count = 0;
    double pctValDistribu = 0;
    double totalPctDistribute = 0;
    DecimalFormat decimalFormate = new DecimalFormat("##.##");
    double[] arr = new double[fileDataArea.getRowCount()];

    try
    {
      for(int i = 0; i < totalRowInTable; i++ )
      {
	Object checkBox = fileDataArea.getValueAt(i, 0);
	boolean isChecked = ((Boolean)checkBox).booleanValue();
	if(isChecked)
	{
	  count++;
	  double tablePctDistValue = Double.parseDouble(fileDataArea.getValueAt(i, pctDistColIndex).toString().trim());
	  pctValDistribu = Double.parseDouble(decimalFormate.format(tablePctDistValue));
	  arr[i] = pctValDistribu;
	}
      }
    }
    catch(Exception ex)
    {
      JOptionPane.showMessageDialog(null, "Percentage should be in Numeric.", "ERROR", JOptionPane.ERROR_MESSAGE);
    }
    
    if(count > 0)
      pctValDistribu = 100/(double)count;
    
    pctValDistribu = Double.parseDouble(decimalFormate.format(pctValDistribu));
    for(int i = 0; i < fileDataArea.getRowCount(); i++)
    {
      Object checkBox = fileDataArea.getValueAt(i, 0);
      boolean isChecked = ((Boolean)checkBox).booleanValue();
      if(isChecked)
      {
        if(chkDistributeLoad.isSelected() == true)
        { 
          fileDataArea.setValueAt(pctValDistribu, i, pctDistColIndex);
          totalPctDistribute = totalPctDistribute + pctValDistribu ;
        }
        else
        {
          fileDataArea.setValueAt(arr[i], i, pctDistColIndex);
        }
      }        
      else
	fileDataArea.setValueAt("0.0", i, pctDistColIndex);
    }
    if(totalPctDistribute != 100 && count > 0)
    {
      double diffVal = 100 - totalPctDistribute;
      if(chkDistributeLoad.isSelected() == true)
        fileDataArea.setValueAt(pctValDistribu + diffVal, 0, pctDistColIndex);
    }
  }
  
  /* 
   * this class is used to handle events on HeaderCheckbox
   */

  class HeaderCheckBoxHandler implements TableModelListener
  {
    private final JTable table;
    private final int targetColumnIndex;
    public HeaderCheckBoxHandler(JTable table, int index)
    {
      this.table = table;
      this.targetColumnIndex = index;
    }

    public void tableChanged(TableModelEvent e)
    {
      if(e.getType()==TableModelEvent.UPDATE && e.getColumn()==targetColumnIndex)
      {
        int vci = table.convertColumnIndexToView(targetColumnIndex);
        TableColumn column = table.getColumnModel().getColumn(vci);
        if(!Status.INDETERMINATE.equals(column.getHeaderValue()))
        {
          column.setHeaderValue(Status.INDETERMINATE);
        }
        else
        {
          boolean selected = true, deselected = true;
          TableModel tableModelIs = table.getModel();
          for(int i=0; i<tableModelIs.getRowCount(); i++)
          {
            Boolean b = (Boolean)tableModelIs.getValueAt(i, targetColumnIndex);
            selected &= b; deselected &= !b;
            if(selected==deselected) return;
          }
          
          if(selected)
          {
            column.setHeaderValue(Status.SELECTED);
          }
          else if(deselected)
          {
            column.setHeaderValue(Status.DESELECTED);
          }
          else
          {
            return;
          }
        }
        JTableHeader header = table.getTableHeader();
        header.repaint(header.getHeaderRect(vci));
      }
    }
  }

  class ComponentIcon implements Icon
  {
    private final JComponent cmp;

    public ComponentIcon(JComponent cmp)
    {
      this.cmp = cmp;
    }

    public int getIconWidth()
    {
      return cmp.getPreferredSize().width;
    }

    public int getIconHeight()
    {
      return cmp.getPreferredSize().height;
    }

    public void paintIcon(Component c, Graphics g, int x, int y)
    {
      SwingUtilities.paintComponent(g, cmp, (Container)c, x, y, getIconWidth(), getIconHeight());
    }
  }

  enum Status
  {
    SELECTED, DESELECTED, INDETERMINATE
  } // setting fixed values to enum type for Header Checkbox state

}
