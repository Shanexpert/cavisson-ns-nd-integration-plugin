package pacScenario;

/**----------------------------------------------------------------------------
 * Name       FileAndDirChooser.java
 * Purpose    This file show the list of file and directories under given path
 * @author    Sangeeta sahu
 * Modification History
 *---------------------------------------------------------------------------**/

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Arrays;
import java.util.Comparator;

import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.ListSelectionModel;
import javax.swing.SwingConstants;
import javax.swing.UIManager;
import javax.swing.border.Border;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;
import javax.swing.table.TableColumnModel;
import javax.swing.table.TableModel;

import pacServletClient.ServletReport;
import sun.awt.shell.ShellFolder;

public class FileAndDirChooser implements ListSelectionListener, ActionListener
{
  private boolean DEBUG = false;
  String className = "FileAndDirChooser";
  JTable table;
  MyTableModel tableModel;
  JTextField txtPathOfDir = null;
  JButton btnOK = null;
  JButton btnCancel = null;
  Object[][] listOfFileInfo = null;
  Object[][] tempListOfFileInfo = null;
  boolean ACENDING = true;
  private Object[][] data = null;
  private URL imageURL = null;
  String currentDir = "";
  String optionOfFileOrDir = "D"; //this is to open Dialog in file or directory mode.
  String[] columnNames = {"File Name", "File Size", "File Type", "Last Modified"};
  String columnType[] = {"String", "Numeric", "String", "String"};
  TableCellRenderer renderer;
  TableColumnModel columnModel;
  TableColumn column;
  JComponentTableHeaderRenderer hedrenderer;
  int lastSortedColumn = 0;
  JTableHeader header = null;
  String permittedPath = ""; //this will not allow user to go parent Directory of this  path.
  SchAddGeneratorGUI schAddGeneratorGUI;
  boolean flagOfNotShowingBackButton = true; //this for not showing dirctory "..." where permission is not given
  Border headerBorder = UIManager.getBorder("TableHeader.cellBorder");

  Icon upIcon = new ImageIcon(getUpArrowImage()); // for showing sorting status.
  Icon downIcon = new ImageIcon(getDownArrowImage()); // for showing sorting status.
  JLabel upLabel = null; //For showing icon of sorting status 
  JLabel downLabel = null; // For showing icon of sorting status
  JLabel blankLbl = null; // for removing icon 
  JDialog dialog;
  ImageIcon fileIcon = null;
  JLabel fileInfo = null;
  int noOdDir = 0;

  //this is  to update table data when user click on header to short table data or click in directory icon.
  public void updateTable(String path)
  {
    StringBuffer errMsg = new StringBuffer();
    ServletReport servletReportObj = new ServletReport();

    listOfFileInfo = servletReportObj.getListOfFileAndDir(SchMain.urlCodeBase, "../NetstormServlet", path, true, errMsg);

    if(listOfFileInfo == null)
    {
      JOptionPane.showMessageDialog(dialog, "Path does not exist. ");
      return;
    }

    noOdDir = 0;
    for(int i = 0; i < listOfFileInfo.length; i++)
    {
      if(listOfFileInfo[i][0].equals("D"))
        noOdDir++;
    }

    if(permittedPath.equals(path) || (permittedPath + "/").equals(path))
      flagOfNotShowingBackButton = true;
    else
      flagOfNotShowingBackButton = false;

    tempListOfFileInfo = getSortedDataForColumn(listOfFileInfo, noOdDir, lastSortedColumn, ACENDING, flagOfNotShowingBackButton);
    data = new Object[tempListOfFileInfo.length][4];
    String extention = "";
    for(int i = 0; i < tempListOfFileInfo.length; i++)
    {
      extention = tempListOfFileInfo[i][1].toString().substring((tempListOfFileInfo[i][1].toString().indexOf(".") + 1), tempListOfFileInfo[i][1].toString().length());
      fileIcon = getIconOfFileType(tempListOfFileInfo[i][0].toString(), extention.trim());
      fileInfo = new JLabel(tempListOfFileInfo[i][1].toString(), fileIcon, JLabel.LEFT);

      data[i][0] = fileInfo;
      data[i][1] = tempListOfFileInfo[i][3];
      data[i][2] = tempListOfFileInfo[i][4];
      data[i][3] = tempListOfFileInfo[i][5];
    }

    tableModel = new MyTableModel(columnNames, data);
    table.setModel(tableModel);

    renderer = new JComponentTableCellRenderer(true);

    columnModel = table.getColumnModel();
    column = columnModel.getColumn(0);
    column.setCellRenderer(renderer);

    column = columnModel.getColumn(1);
    column.setPreferredWidth(10);

    hedrenderer = new JComponentTableHeaderRenderer();
    for(int i = 0; i < columnNames.length; i++)
    {
      column = columnModel.getColumn(i);
      column.setHeaderRenderer(hedrenderer);
      if(i == lastSortedColumn)
      {
        if(ACENDING)
        {
          upLabel = new JLabel(columnNames[lastSortedColumn], upIcon, JLabel.CENTER);
          upLabel.setHorizontalTextPosition(SwingConstants.LEFT);
          upLabel.setBorder(headerBorder);
          column.setHeaderValue(upLabel);
        }
        else
        {
          JLabel downLabel = new JLabel(columnNames[lastSortedColumn], downIcon, JLabel.CENTER);
          downLabel.setHorizontalTextPosition(SwingConstants.LEFT);
          downLabel.setBorder(headerBorder);
          column.setHeaderValue(downLabel);
        }
      }
      else
      {
        blankLbl = new JLabel(columnNames[i], JLabel.CENTER);
        blankLbl.setBorder(headerBorder);
        column.setHeaderValue(blankLbl);
      }
    }

    txtPathOfDir.setText(path);
    currentDir = txtPathOfDir.getText();
  }

  public FileAndDirChooser(SchAddGeneratorGUI schAddGeneratorGUI, String Path, String optionOfFileOrDir)
  {
    this.schAddGeneratorGUI = schAddGeneratorGUI;
    this.optionOfFileOrDir = optionOfFileOrDir;
    permittedPath = Path;
    currentDir = Path;

    if(permittedPath.equals(""))
      permittedPath = "/";

    JOptionPane optionPane = new JOptionPane();
    optionPane.removeAll();
    dialog = optionPane.createDialog(dialog, "hello");
    optionPane.setBackground(new Color(243, 251, 254));
    setScreenSize(53, 59);
    dialog.setResizable(true);
    if(optionOfFileOrDir.equals("F"))
      dialog.setTitle("Select Generator File");
    else
      dialog.setTitle("Select Generator Direcotry");

    try
    {
      imageURL = new URL(SchMain.urlCodeBase + "../images/logo_TitleIcon.png");
    }
    catch(MalformedURLException e)
    {
      e.printStackTrace();
    }
    dialog.setIconImage(new ImageIcon(imageURL).getImage());

    optionPane.add(getCenterPanel());
    dialog.setVisible(true);
    //setDefaultCloseOperation(DISPOSE_ON_CLOSE);
    dialog.setFocusable(true);

  }

  public JPanel getCenterPanel()
  {
    StringBuffer errMsg = new StringBuffer();
    ServletReport servletReportObj = new ServletReport();

    listOfFileInfo = servletReportObj.getListOfFileAndDir(SchMain.urlCodeBase, "../NetstormServlet", currentDir, true, errMsg);

    for(int i = 0; i < listOfFileInfo.length; i++)
    {
      if(listOfFileInfo[i][0].equals("D"))
        noOdDir++;
    }

    tempListOfFileInfo = getSortedDataForColumn(listOfFileInfo, noOdDir, lastSortedColumn, ACENDING, flagOfNotShowingBackButton);
    data = new Object[tempListOfFileInfo.length][4];
    String extention = "";
    for(int i = 0; i < tempListOfFileInfo.length; i++)
    {
      extention = tempListOfFileInfo[i][1].toString().substring((tempListOfFileInfo[i][1].toString().indexOf(".") + 1), tempListOfFileInfo[i][1].toString().length());

      fileIcon = getIconOfFileType(tempListOfFileInfo[i][0].toString(), extention.trim());
      fileInfo = new JLabel(tempListOfFileInfo[i][1].toString(), fileIcon, JLabel.LEFT);

      data[i][0] = fileInfo;
      data[i][1] = tempListOfFileInfo[i][3];
      data[i][2] = tempListOfFileInfo[i][4];
      data[i][3] = tempListOfFileInfo[i][5];
    }

    tableModel = new MyTableModel(columnNames, data);
    table = new JTable(tableModel)
    {
      public Component prepareRenderer(TableCellRenderer renderer, int rowIndex, int vColIndex)
      {
        try
        {
          Component c = super.prepareRenderer(renderer, rowIndex, vColIndex);
          c.setForeground(Color.black);
          return c;
        }
        catch(Exception e)
        {
          //          Log.stackTraceLog(className, "prepareRenderer", "", "", "Exception", e);
          return null;
        }
      }
    };
    table.setPreferredScrollableViewportSize(new Dimension(500, 70));
    table.setFillsViewportHeight(true);
    table.setRowHeight(22);
    table.setSelectionBackground(new Color(209, 232, 246));
    table.setIntercellSpacing(new Dimension(0, 4));
    table.setShowGrid(false);

    header = table.getTableHeader();
    header.addMouseListener(new MouseAdapter()
    {
      public void mouseClicked(MouseEvent evt)
      {
        JTable table = ((JTableHeader)evt.getSource()).getTable();
        TableColumnModel colModel = table.getColumnModel();

        // The index of the column whose header was clicked
        int vColIndex = colModel.getColumnIndexAtX(evt.getX());
        int mColIndex = table.convertColumnIndexToModel(vColIndex);

        // Return if not clicked on any column header
        if(vColIndex == -1)
        {
          return;
        }
        lastSortedColumn = vColIndex;
        sortTableDataOnHeaderClick(vColIndex);
        hedrenderer = new JComponentTableHeaderRenderer();

        TableColumn column0 = null;
        //this is for showing icon on header click.
        for(int j = 0; j < columnNames.length; j++)
        {
          column0 = colModel.getColumn(j);
          column0.setHeaderRenderer(hedrenderer);
          if(j == vColIndex)
          {
            if(ACENDING)
            {
              upLabel = new JLabel(columnNames[j], upIcon, JLabel.CENTER);
              upLabel.setHorizontalTextPosition(SwingConstants.LEFT);
              upLabel.setBorder(headerBorder);
              column0.setHeaderValue(upLabel);
            }
            else
            {
              downLabel = new JLabel(columnNames[j], downIcon, JLabel.CENTER);
              downLabel.setHorizontalTextPosition(SwingConstants.LEFT);
              downLabel.setBorder(headerBorder);
              column0.setHeaderValue(downLabel);
            }
          }
          else
          {
            blankLbl = new JLabel(columnNames[j], JLabel.CENTER);
            blankLbl.setBorder(headerBorder);
            column0.setHeaderValue(blankLbl);
          }
        }
      }
    });

    header.setPreferredSize(new Dimension(20, 20));
    header.setBackground(new Color(209, 232, 246));
    header.setReorderingAllowed(false);

    table.addMouseListener(new MouseAdapter()
    {
      public void mouseClicked(MouseEvent e)
      {
        if(e.getClickCount() == 2)
        {
          if(table.getSelectedRow() != -1)
          {
            JLabel temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
            String dirctory = temp.getText();
            if(dirctory.equals("...")) //if clicked on back button
            {
              if(!currentDir.endsWith("/"))
              {
                if(currentDir.equals(permittedPath))
                {
                  JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                  return;
                }
                updateTable(currentDir.substring(0, currentDir.lastIndexOf("/")));
              }
              else
              {
                String tempStr = currentDir.substring(0, currentDir.lastIndexOf("/"));
                if(tempStr.equals(permittedPath))
                {
                  JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                  return;
                }
                updateTable(tempStr.substring(0, tempStr.lastIndexOf("/")));
              }
            }
            else if(table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
            {
              if(currentDir.endsWith("/"))
                updateTable(currentDir + "" + temp.getText());
              else
                updateTable(currentDir + "/" + temp.getText());
            }
            else
            //this is to respond on file click
            {
              if(optionOfFileOrDir.equals("F"))
              {
                String Path = currentDir;
                if(table.getSelectedRow() != -1)
                {
                  if(!table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
                  {
                    temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
                    Path += "/" + temp.getText();
                    schAddGeneratorGUI.txtGnName.setText(Path);
                    boolean fileuploaded = schAddGeneratorGUI.fileUploadFromServer(Path);
                    SchAddGeneratorGUI.isNewFileDownload = true;

                    if(fileuploaded)
                      dialog.dispose();
                  }
                }
              }
            }
          }
        }
      }
    });

    table.addKeyListener(new KeyAdapter()
    { // this is to respond for searching starting with pressed alphabets.
          public void keyPressed(KeyEvent ke)
          {
            if(ke.getKeyCode() == KeyEvent.VK_ENTER)
            {
              if(table.getSelectedRow() != -1)
              {
                JLabel temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
                String dirctory = temp.getText();
                if(dirctory.equals("..."))
                {
                  if(!currentDir.endsWith("/"))
                  {
                    if(currentDir.equals(permittedPath))
                    {
                      JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                      return;
                    }
                    updateTable(currentDir.substring(0, currentDir.lastIndexOf("/")));
                  }
                  else
                  {
                    String tempStr = currentDir.substring(0, currentDir.lastIndexOf("/"));
                    if(tempStr.equals(permittedPath))
                    {
                      JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                      return;
                    }
                    updateTable(tempStr.substring(0, tempStr.lastIndexOf("/")));
                  }
                }
                else if(table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
                {
                  if(currentDir.endsWith("/"))
                    updateTable(currentDir + "" + temp.getText());
                  else
                    updateTable(currentDir + "/" + temp.getText());
                }
                else
                {
                  if(optionOfFileOrDir.equals("F"))
                  {
                    String Path = currentDir;
                    if(table.getSelectedRow() != -1)
                    {
                      if(!table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
                      {
                        temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
                        Path += "/" + temp.getText();
                        schAddGeneratorGUI.txtGnName.setText(Path);
                        boolean fileuploaded = schAddGeneratorGUI.fileUploadFromServer(Path);
                        SchAddGeneratorGUI.isNewFileDownload = true;

                        if(fileuploaded)
                          dialog.dispose();
                      }
                    }
                  }
                }
              }
            }
            else if(ke.getKeyCode() == KeyEvent.VK_BACK_SPACE)
            {
              if(!currentDir.endsWith("/"))
              {
                if(currentDir.equals(permittedPath))
                {
                  JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                  return;
                }
                updateTable(currentDir.substring(0, currentDir.lastIndexOf("/")));
              }
              else
              {
                String tempStr = currentDir.substring(0, currentDir.lastIndexOf("/"));
                if(tempStr.equals(permittedPath))
                {
                  JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
                  return;
                }
                updateTable(tempStr.substring(0, tempStr.lastIndexOf("/")));
              }
            }
            else
            {
              int count = 0;
              int startIndex = table.getSelectedRow() + 1;
              if(startIndex == table.getRowCount())
                startIndex = 0;
              for(int i = startIndex; true; i++)
              {
                if(i >= table.getRowCount())
                  i = 0;
                JLabel temp = (JLabel)table.getValueAt(i, 0);
                if(temp.getText().substring(0, 1).equalsIgnoreCase(ke.getKeyChar() + ""))
                {
                  table.changeSelection(i, i, false, false);
                  break;
                }
                count++;
                if(count == table.getRowCount())
                  break;
              }
            }
          }
        });

    renderer = new JComponentTableCellRenderer(true);

    columnModel = table.getColumnModel();
    column = columnModel.getColumn(0);
    column.setCellRenderer(renderer);

    hedrenderer = new JComponentTableHeaderRenderer();
    for(int i = 0; i < columnNames.length; i++)
    {
      column = columnModel.getColumn(i);
      column.setHeaderRenderer(hedrenderer);
      if(i == 0)
      {
        upLabel = new JLabel(columnNames[i], upIcon, JLabel.CENTER);
        upLabel.setHorizontalTextPosition(SwingConstants.LEFT);
        upLabel.setBorder(headerBorder);
        column.setHeaderValue(upLabel);
      }
      else
      {
        blankLbl = new JLabel(columnNames[i], JLabel.CENTER);
        blankLbl.setBorder(headerBorder);
        column.setHeaderValue(blankLbl);
      }
    }

    column = columnModel.getColumn(1);
    column.setPreferredWidth(10);

    ListSelectionModel listMod = table.getSelectionModel();
    listMod.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
    listMod.addListSelectionListener(this);

    //Create the scroll pane and add the table to it.
    JScrollPane scrollPane = new JScrollPane(table);
    scrollPane.setComponentOrientation(java.awt.ComponentOrientation.LEFT_TO_RIGHT);
    scrollPane.setAlignmentX(JScrollPane.BOTTOM_ALIGNMENT);
    scrollPane.setBackground(new Color(209, 232, 246));

    txtPathOfDir = new JTextField(33);
    txtPathOfDir.setPreferredSize(new Dimension(53, 25));

    //servletReportObj = new ServletReport();
    //String workPath = servletReportObj.getWorkPath(ScriptMain.urlCodeBase, "../NetstormServlet", errMsg);

    //System.out.println("workPath = " + workPath);
    txtPathOfDir.setText(currentDir);
    txtPathOfDir.addKeyListener(new KeyAdapter()
    {
      public void keyPressed(KeyEvent ke)
      {
        if(ke.getKeyCode() == KeyEvent.VK_ENTER)
        {
          if(!txtPathOfDir.getText().startsWith(permittedPath))
          {
            JOptionPane.showMessageDialog(dialog, "Path sould start with \" " + permittedPath + "/ \"");
            return;
          }
          updateTable(txtPathOfDir.getText());
        }
      }
    });

    currentDir = txtPathOfDir.getText();
    btnOK = new JButton("    OK    ");
    btnOK.addActionListener(this);
    btnCancel = new JButton("Cancel");
    btnCancel.addActionListener(this);

    JPanel lowerPnl = new JPanel();
    lowerPnl.add(btnOK);
    lowerPnl.add(btnCancel);
    lowerPnl.setBackground((new Color(243, 251, 254)));

    JPanel centrePanel = new JPanel();
    centrePanel.setLayout(new BorderLayout(5, 5));

    JPanel upperPnl = new JPanel();
    upperPnl.setLayout(new BorderLayout(5, 5));
    JLabel lable = new JLabel("Generator Path ");
    upperPnl.add(lable, BorderLayout.WEST);
    JLabel blank = new JLabel();
    upperPnl.add(txtPathOfDir, BorderLayout.CENTER);
    upperPnl.setBackground((new Color(243, 251, 254)));
    upperPnl.add(blank, BorderLayout.NORTH);
    upperPnl.add(blank, BorderLayout.EAST);
    upperPnl.add(blank, BorderLayout.SOUTH);

    centrePanel.add(upperPnl, BorderLayout.NORTH);
    centrePanel.add(scrollPane, BorderLayout.CENTER);
    centrePanel.add(lowerPnl, BorderLayout.SOUTH);
    centrePanel.setBackground((new Color(243, 251, 254)));
    return centrePanel;
  }

  class MyTableModel extends AbstractTableModel
  {
    String[] columnNames = null;
    Object[][] data;

    public MyTableModel(String[] columnNames, Object[][] data)
    {
      this.columnNames = columnNames;
      this.data = data;
    }

    public Class getColumnClass(int column)
    {
      if(column >= 0 && column <= getColumnCount())
      {
        return getValueAt(0, column).getClass();
      }
      else
        return Object.class;
    }

    public int getColumnCount()
    {
      return columnNames.length;
    }

    public int getRowCount()
    {
      return data.length;
    }

    public String getColumnName(int col)
    {
      return columnNames[col];
    }

    public Object getValueAt(int row, int col)
    {
      return data[row][col];
    }

    /*
     * JTable uses this method to determine the default renderer/
     * editor for each cell.  If we didn't implement this method,
     * then the last column would contain text ("true"/"false"),
     * rather than a check box.
     */

    /*public Class getColumnClass(int c)
    {
      return getValueAt(0, c).getClass();
    }*/

    /*
     * Don't need to implement this method unless your table's
     * editable.
     */
    public boolean isCellEditable(int row, int col)
    {
      //Note that the data/cell address is constant,
      //no matter where the cell appears onscreen.
      return false;
    }

    /*
     * Don't need to implement this method unless your table's
     * data can change.
     */
    public void setValueAt(Object value, int row, int col)
    {
      if(DEBUG)
      {
        System.out.println("Setting value at " + row + "," + col + " to " + value + " (an instance of " + value.getClass() + ")");
      }

      data[row][col] = value;
      fireTableCellUpdated(row, col);

      if(DEBUG)
      {
        System.out.println("New value of data:");
        printDebugData();
      }
    }

    private void printDebugData()
    {
      int numRows = getRowCount();
      int numCols = getColumnCount();

      for(int i = 0; i < numRows; i++)
      {
        System.out.print("    row " + i + ":");
        for(int j = 0; j < numCols; j++)
        {
          System.out.print("  " + data[i][j]);
        }
        System.out.println();
      }
      System.out.println("--------------------------");
    }
  }

  /**
   * Create the GUI and show it.  For thread safety,
   * this method should be invoked from the
   * event-dispatching thread.
   */

  public void valueChanged(ListSelectionEvent e)
  {
    int maxRows;
    int[] selRows;
    Object value;

    if(!e.getValueIsAdjusting())
    {
      selRows = table.getSelectedRows();
      if(selRows.length > 0)
      {
        for(int i = 0; i < 3; i++)
        {
          // get Table data
          TableModel tm = table.getModel();
          value = tm.getValueAt(selRows[0], i);
          //System.out.println("Selection : " + value );
        }
        //System.out.println();
      }
    }
  }

  public void setScreenSize(int width, int height)
  {
    //Get the screen size
    Toolkit toolkit = Toolkit.getDefaultToolkit();
    Dimension screenSize = toolkit.getScreenSize();

    //Calculate the frame location
    int screenWidth = (screenSize.width * width) / 100;
    int screenHeight = (screenSize.height * height) / 100;
    int x = (screenSize.width - screenWidth) / 2;
    int y = (screenSize.height - screenHeight) / 2;
    dialog.setSize(screenWidth, screenHeight);

    //Set the new frame location
    dialog.setLocation(x, y);
  }

  public void actionPerformed(ActionEvent e)
  {
    if(e.getSource().getClass().toString().equals("class javax.swing.JButton"))
    {
      JButton tempButton = (JButton)e.getSource();
      if(tempButton.getText().trim().equals("Cancel"))
      {
        dialog.dispose();
      }
      else
      //  if clicked on OK button
      {
        String Path = currentDir;
        JLabel temp = null;
        if(optionOfFileOrDir.equals("D"))
        {
          if(table.getSelectedRow() != -1)
          {
            if(table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
            {
              temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
              Path += "/" + temp.getText();
            }
          }
        }
        else
        {
          if(table.getSelectedRow() != -1)
          {
            if(!table.getValueAt(table.getSelectedRow(), 2).toString().trim().equals("File Folder"))
            {
              temp = (JLabel)table.getValueAt(table.getSelectedRow(), 0);
              Path += "/" + temp.getText();
            }
            else
            {
              JOptionPane.showMessageDialog(dialog, "Please select a file ");
              return;
            }
          }
          else
          {
            JOptionPane.showMessageDialog(dialog, "Please select a file ");
            return;
          }
        }
        schAddGeneratorGUI.txtGnName.setText(Path);
        boolean fileuploaded = schAddGeneratorGUI.fileUploadFromServer(Path);
        SchAddGeneratorGUI.isNewFileDownload = true;
        //System.out.println("user selected path = "+Path);
        if(fileuploaded)
          dialog.dispose();
      }
    }
  }

  //This is to get icons from system related to file type.
  public ImageIcon getIconOfFileType(String FileOrDir, String extension)
  {
    ImageIcon fileIcon = null;
    File fileTemp = null;
    if(FileOrDir.equals("D"))
    {
      try
      {
        imageURL = new URL(SchMain.urlCodeBase + "../images/folder-closed.gif");
      }
      catch(MalformedURLException e)
      {
        e.printStackTrace();
      }
      fileIcon = new ImageIcon(imageURL);
    }
    else
    {
      try
      {
        fileTemp = File.createTempFile("icon", "." + extension.trim());
        ShellFolder shellFolder = ShellFolder.getShellFolder(fileTemp);
        fileIcon = new ImageIcon(shellFolder.getIcon(false));
        //Delete the temporary file  
        fileTemp.delete();
      }
      catch(Exception e)
      {
        try
        {
          imageURL = new URL(SchMain.urlCodeBase + "../images/txtFile.png");
        }
        catch(MalformedURLException e1)
        {
          e1.printStackTrace();
        }
        fileIcon = new ImageIcon(imageURL);
      }
    }
    return fileIcon;
  }

  //this is to get up arrow image url.
  private URL getUpArrowImage()
  {
    try
    {
      imageURL = new URL(SchMain.urlCodeBase + "../images/upArrow.gif");
    }
    catch(MalformedURLException e)
    {
      e.printStackTrace();
    }
    return imageURL;
  }

  //this is to get down arrow image url.
  private URL getDownArrowImage()
  {
    try
    {
      imageURL = new URL(SchMain.urlCodeBase + "../images/downArrow.gif");
    }
    catch(MalformedURLException e)
    {
      e.printStackTrace();
    }
    return imageURL;
  }

  //This is to get sorted data for selected column on header column click
  public Object[][] getSortedDataForColumn(Object[][] list, int noOdDir, int column, boolean acedending, boolean flagOfNotShowingBackButton)
  {
    String[][] tempDirs = new String[noOdDir - 1][6];
    String[][] tempFiles = new String[list.length - noOdDir][6];
    int indexOFColumn = 1; // index of column which need to sort.
    if(column == 0)
      indexOFColumn = 1;
    else if(column == 1)
      indexOFColumn = 3;
    else if(column == 2)
      indexOFColumn = 4;
    else if(column == 3)
      indexOFColumn = 5;

    for(int i = 1; i < noOdDir; i++)
    {
      for(int k = 0; k < 6; k++)
        tempDirs[i - 1][k] = list[i][k].toString();
    }
    Arrays.sort(tempDirs, new ColumnComparator(indexOFColumn, acedending));

    int j = 0;
    for(int i = noOdDir; i < list.length; i++)
    {
      for(int k = 0; k < 6; k++)
        tempFiles[j][k] = list[i][k].toString();
      j++;
    }

    //System.out.println("tempDirs = "+tempDirs.length+" , tempFiles = "+tempFiles.length);
    int kk = 0;
    int ll = 1;
    if(flagOfNotShowingBackButton)
    {
      kk = 1;
      ll = 0;
    }

    Arrays.sort(tempFiles, new ColumnComparator(indexOFColumn, acedending));
    Object temp[][] = new Object[list.length - kk][6];

    if(!flagOfNotShowingBackButton)
    {
      temp[0][0] = "D";
      temp[0][1] = "...";
      temp[0][2] = "folder-closed.gif";
      temp[0][3] = "";
      temp[0][4] = "";
      temp[0][5] = "";
    }

    j = 0;
    if(acedending)
    {
      for(int i = 0; i < tempDirs.length; i++)
      {
        for(int k = 0; k < 6; k++)
          temp[j + ll][k] = tempDirs[i][k].toString();
        j++;
      }
      for(int i = 0; i < tempFiles.length; i++)
      {
        for(int k = 0; k < 6; k++)
          temp[j + ll][k] = tempFiles[i][k].toString();
        j++;
      }
    }
    else
    {
      for(int i = 0; i < tempFiles.length; i++)
      {
        for(int k = 0; k < 6; k++)
          temp[j + ll][k] = tempFiles[i][k].toString();
        j++;
      }
      for(int i = 0; i < tempDirs.length; i++)
      {
        for(int k = 0; k < 6; k++)
          temp[j + ll][k] = tempDirs[i][k].toString();
        j++;
      }
    }
    return temp;
  }

  public void sortTableDataOnHeaderClick(int column)
  {
    Object tempListOfFileInfo[][] = null;
    boolean ASC = true;
    if(ACENDING == true)
    {
      ASC = false;
      ACENDING = false;
    }
    else
    {
      ASC = true;
      ACENDING = true;
    }

    tempListOfFileInfo = getSortedDataForColumn(listOfFileInfo, noOdDir, column, ASC, flagOfNotShowingBackButton);
    if(tempListOfFileInfo == null)
    {
      JOptionPane.showMessageDialog(dialog, "Path does not exist. ");
      return;
    }
    data = new Object[tempListOfFileInfo.length][4];
    String extention = "";
    for(int i = 0; i < tempListOfFileInfo.length; i++)
    {
      extention = tempListOfFileInfo[i][1].toString().substring((tempListOfFileInfo[i][1].toString().indexOf(".") + 1), tempListOfFileInfo[i][1].toString().length());
      fileIcon = getIconOfFileType(tempListOfFileInfo[i][0].toString(), extention.trim());
      fileInfo = new JLabel(tempListOfFileInfo[i][1].toString(), fileIcon, JLabel.LEFT);

      data[i][0] = fileInfo;
      data[i][1] = tempListOfFileInfo[i][3];
      data[i][2] = tempListOfFileInfo[i][4];
      data[i][3] = tempListOfFileInfo[i][5];
    }

    tableModel = new MyTableModel(columnNames, data);
    table.setModel(tableModel);
    TableCellRenderer renderer = new JComponentTableCellRenderer(true);

    TableColumnModel columnModel = table.getColumnModel();
    TableColumn column1 = columnModel.getColumn(0);
    column1.setCellRenderer(renderer);
    TableColumn column2 = columnModel.getColumn(1);
    column2.setPreferredWidth(10);
  }

  public static void main(String[] args)
  {
    //FileAndDirChooser obj = new FileAndDirChooser("c:\\", "D");
  }
}

//Class that extends Comparator
class ColumnComparator implements Comparator
{
  int columnToSort;
  boolean ascending;

  ColumnComparator(int columnToSort, boolean ascending)
  {
    this.columnToSort = columnToSort;
    this.ascending = ascending;
  }

  //overriding compare method
  public int compare(Object o1, Object o2)
  {
    String[] row1 = (String[])o1;
    String[] row2 = (String[])o2;
    //compare the columns to sort
    int cmp = 0;
    if(columnToSort != 3)
    {
      cmp = row1[columnToSort].compareToIgnoreCase(row2[columnToSort]);
      return ascending ? cmp : -cmp;
    }
    else
    {
      double d1 = 0.0;
      double d2 = 0.0;
      if(!row1[columnToSort].equals(""))
        d1 = Double.parseDouble(row1[columnToSort]);
      if(!row2[columnToSort].equals(""))
        d2 = Double.parseDouble(row2[columnToSort]);
      if(d1 < d2)
        cmp = -1;
      else if(d1 > d2)
        cmp = 1;
      else
        return 0;
      return ascending ? cmp : -cmp;
    }
  }
}

class JComponentTableCellRenderer implements TableCellRenderer
{
  boolean isBordered = true;
  JLabel ll;

  public JComponentTableCellRenderer(boolean isBordered)
  {
    this.isBordered = isBordered;
  }

  public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
  {

    if(isBordered)
    {
      if(isSelected)
      {
        ll = (JLabel)value;
        ll.setOpaque(true);
        ll.setBackground(table.getSelectionBackground());
        ll.setForeground(Color.red);
      }
      else
      {
        ll = (JLabel)value;
        ll.setOpaque(true);
        ll.setBackground(table.getBackground());
        ll.setForeground(Color.black);
      }
    }
    return ll;
  }
}

class JComponentTableHeaderRenderer implements TableCellRenderer
{
  public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
  {
    return (JComponent)value;
  }
}
