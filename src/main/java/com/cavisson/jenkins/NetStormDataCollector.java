/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;
import hudson.model.Run;
import java.text.SimpleDateFormat;
import java.util.Date;


/**
 *
 * @author preety.yadav
 */
public class NetStormDataCollector {
    private static final Logger LOG = Logger.getLogger(NetStormDataCollector.class.getName());
  private static final String[] METRIC_PATHS =  new String[]{"Transactions Started/Second", "Transactions Completed/Second", "Transactions Successful/Second", "Average Transaction Response Time (Secs)", "Transactions Completed","Transactions Success" };
  private static final int graphId [] = new int[]{7,8,9,3,5,6};
  private static final int groupId [] = new int[]{6,6,6,6,6,6};
  
  private final NdConnectionManager ndConnection;
  private final NetStormConnectionManager connection;
  private final NSNDIntegrationConnectionManager nsNdConnection;
  private final Run<?, ?> build;
  private final int TEST_RUN;
  private final String TEST_MODE;
  private boolean isNDE = false;
  private boolean isIntegrated = false;
  private String duration;

  public NetStormDataCollector(final NetStormConnectionManager connection, final Run build, int TestRun, String testMode) 
  {
    this.connection = connection;
    this.build = build;  
    this.TEST_RUN = TestRun;
    this.TEST_MODE = testMode;
    this.ndConnection = null;
    this.nsNdConnection = null;
  }

  public NetStormDataCollector(final NdConnectionManager connection, final Run<?, ?> build,int TestRun, String testMode, boolean isCM, String duration) 
  {
    this.ndConnection = connection;
    this.build = build;  
    this.TEST_RUN = TestRun;
    this.TEST_MODE = testMode;
    this.isNDE = isCM;
    this.connection = null;
    this.duration = duration;
    this.nsNdConnection = null;
  }
  
  public NetStormDataCollector(final NSNDIntegrationConnectionManager connection, final Run build, int TestRun, String testMode, boolean isIntegrated, String duration) 
  {
    this.connection = null;
    this.ndConnection = null;
    this.nsNdConnection = connection;
    this.build = build;  
    this.TEST_RUN = TestRun;
    this.TEST_MODE = testMode;
     this.duration = duration;
    this.isIntegrated = isIntegrated;
  }

  public static String[] getAvailableMetricPaths() 
  {
    return METRIC_PATHS;
  }

  /** Parses the specified reports into {@link}s.
   * @return  */
  public NetStormReport createReportFromMeasurements() throws Exception
  {
    long buildStartTime = build.getTimeInMillis();
    String durationInMinutes = calculateDurationToFetch(buildStartTime,  System.currentTimeMillis());

    try
    {
    
    	NetStormReport adReport;
    	MetricDataContainer metricDataContainer;
    	 
    	if(!isNDE && !isIntegrated)
    	{
           LOG.log(Level.INFO, "Inside for generating a netstrom test  = ");
           adReport = new NetStormReport(buildStartTime, durationInMinutes);
           metricDataContainer = connection.fetchMetricData(connection, METRIC_PATHS, durationInMinutes+"", groupId, graphId, TEST_RUN, TEST_MODE);
    	}
        else if(isIntegrated && !isNDE)
        {
          System.out.println("test run  -- "+TEST_RUN);
          durationInMinutes = this.duration;
    	  adReport = new NetStormReport(buildStartTime, durationInMinutes);
          adReport.setIsIntegrated(isIntegrated);
    	  metricDataContainer = nsNdConnection.fetchMetricData(nsNdConnection,METRIC_PATHS,durationInMinutes, groupId, graphId,TEST_RUN, TEST_MODE);
          
          String curDateTime = metricDataContainer.getTestReport().getCurrentDateTime();
          String curEndTime = nsNdConnection.getCurEnd();
          SimpleDateFormat trDateFormat = new SimpleDateFormat("MM/dd/yy HH:mm:ss");
          Date startTime = trDateFormat.parse(curDateTime); 
          long startTimeInMillis = startTime.getTime();
          Date endTime = trDateFormat.parse(curEndTime); 
          long endTimeInMillis = endTime.getTime();
    	  durationInMinutes = calculateDurationToFetch(startTimeInMillis, endTimeInMillis);
          adReport.setReportDurationInMinutes(durationInMinutes);
        }
    	else
    	{
        
    	   durationInMinutes = this.duration;
    	   adReport = new NetStormReport(buildStartTime, durationInMinutes, isNDE);
    	   metricDataContainer = ndConnection.fetchMetricData(METRIC_PATHS, durationInMinutes, groupId, graphId, TEST_RUN, TEST_MODE);
         
    	}
    	
       
       LOG.log(Level.INFO, "metrix data container = "+metricDataContainer);
      if(metricDataContainer != null)
        adReport.setCustomHTMLReport(metricDataContainer.getCustomHTMLReport());
      
      ArrayList<MetricData> metricList = metricDataContainer.getMetricDataList();
    
      if(metricList != null)
      {
        for(MetricData metric : metricList )
        {
          if (adReport != null && metric != null) 
            adReport.addMetrics(metric);
        }
      }
      
      LOG.log(Level.INFO, "metrix List....... = "+metricList);
    
      if(metricDataContainer.getMetricBaseLineDataList() != null)
      {
        ArrayList<MetricData> metricBaseLineList = metricDataContainer.getMetricBaseLineDataList();
     
        for(MetricData metric : metricBaseLineList )
        {
          if (adReport != null && metric != null) 
            adReport.addBaseLineMetrics(metric);
        }
      }
       LOG.log(Level.INFO, "get matriv data containfgkd..... = "+metricDataContainer);
      
      LOG.log(Level.INFO, "get matrix prev values....... = "+metricDataContainer.getMetricPreviousDataList());
     
      /*
      *  create a dummy previous data list variable in case of null for testing purpose
      */
      ArrayList<MetricData> metricPreviousListData  = new ArrayList<MetricData>();
      MetricData md = new MetricData();
      metricPreviousListData.add(md);
      metricDataContainer.setMetricPreviousDataList(metricPreviousListData);
      
      if(metricDataContainer.getMetricPreviousDataList() != null)
      {
        ArrayList<MetricData> metricPreviousList = metricDataContainer.getMetricPreviousDataList();
        for(MetricData metric : metricPreviousList )
        {
          if (adReport != null && metric != null) 
            adReport.addPrevoiusMetrics(metric);
        }
      }
      
      /*
      *  add a dummy get test reporting data
      */
      
       
      TestReport testReport = metricDataContainer.getTestReport();
      TestReport testReportND = metricDataContainer.getTestReportND();
      
      
      LOG.log(Level.INFO, "get matrix for testing a report....... = "+testReport);
      if(testReport != null)
      {
        adReport.setTestReport(testReport);
        
      }
      
      if(testReportND != null){
         adReport.setTestReportND(testReportND);        
      }
     
      LOG.log(Level.INFO, "Test Report is generating..........>>>>  = " +testReport);
      LOG.log(Level.INFO, "Test Report is generating ND..........>>>>  = " +testReportND);
      
      
      return adReport;
    }
    catch(Exception e)
    {
      LOG.log(Level.SEVERE, "exception in createReportFromMeasurements" , e); 
      throw e;
    }
  }

  private String calculateDurationToFetch(final Long testStartTime, final Long testEndTime) 
  {   
    long duration =  testEndTime - testStartTime;
    long seconds = duration / 1000;
    long s = seconds % 60;
    long m = (seconds / 60) % 60;
    long h = (seconds / (60 * 60)) % 24;
    return String.format("%d:%02d:%02d", h,m,s);
  }
}
