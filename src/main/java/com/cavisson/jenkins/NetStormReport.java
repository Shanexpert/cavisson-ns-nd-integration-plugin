/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import hudson.model.Run;
import java.net.URLDecoder;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;


/**
 *
 * @author preety.yadav
 */
public class NetStormReport {
    private final Map<String, MetricData> keyedMetricDataMap = new LinkedHashMap<String, MetricData>();
  private final Map<String, MetricData> keyedMetricPreviousDataMap = new LinkedHashMap<String, MetricData>();
  private final Map<String, MetricData> keyedMetricBaseLineDataMap = new LinkedHashMap<String, MetricData>();
  
  private TestReport testReport;   
  private TestReport testReportND;
  private final Long reportTimestamp;
  private  String reportDurationInMinutes;

  private NetStormBuildAction buildAction;
  private NetStormReport lastBuildReport;
  private String customHTMLReport = "NA";
  private boolean isNDE = false;
  private boolean isIntegrated = false;
  private static final Logger logger = Logger.getLogger(NetStormReport.class.getName());
  

  public NetStormReport(final Long timestamp, final String durationInMinutes) 
  {
    this.reportTimestamp = timestamp;
    this.reportDurationInMinutes = durationInMinutes;  
  }
 
  public NetStormReport(final Long timestamp, final String durationInMinutes, final boolean isNDE) 
  {
    this.reportTimestamp = timestamp;
    this.reportDurationInMinutes = durationInMinutes;
    logger.log(Level.INFO, "get name constructor duration    -"+durationInMinutes );
    this.isNDE = isNDE;
    
    
  }
 
  
  public TestReport getTestReport()
  {
    return testReport;
  }

  public void setTestReport(TestReport testReport)
  {
    this.testReport = testReport;
  }
  
  
   public TestReport getTestReportND()
  {
    return testReportND;
  }

  public void setTestReportND(TestReport testReport)
  {
    this.testReportND = testReport;
  }

  public void addMetrics(final MetricData metrics) 
  {
    keyedMetricDataMap.put(metrics.getMetricPath(), metrics);
  }
  
  public void addBaseLineMetrics(final MetricData metrics) 
  {
    keyedMetricBaseLineDataMap.put(metrics.getMetricPath(), metrics);
  }
   
   public void addPrevoiusMetrics(final MetricData metrics) 
  {
    keyedMetricPreviousDataMap.put(metrics.getMetricPath(), metrics);
  }
   
  public MetricData getMetricByKey(final String metricKey, String type)
  {
    MetricData selectedMetric = null;
    if(type == "BASELINE") 
      selectedMetric = keyedMetricBaseLineDataMap.get(metricKey);
    else
      selectedMetric = keyedMetricPreviousDataMap.get(metricKey);
    
    return selectedMetric;
  }
  
  public MetricData getMetricByKey(final String metricKey)
  {
    final MetricData selectedMetric = keyedMetricDataMap.get(metricKey);
    
    if (selectedMetric == null) {
      throw new IllegalArgumentException("Provided Metric Key is not available, tried to select; " + metricKey);
    }
    
    return selectedMetric;
  }

  public List<MetricData> getMetricsListBasedOnType(String type)
  {
    if(type.equals("BASELINE"))
     return new ArrayList<MetricData>(keyedMetricBaseLineDataMap.values());
    else
     return new ArrayList<MetricData>(keyedMetricPreviousDataMap.values());
  }
  
  public List<MetricData> getMetricsList()
  {
    return new ArrayList<MetricData>(keyedMetricDataMap.values());
  }

  public double getAverageForMetric(final String metricKey) 
  {
    try
    {
      final MetricData selectedMetric = getMetricByKey(metricKey);
   
      double calculatedSum = 0;
    
      for (MetricValues value : selectedMetric.getMetricValues()) 
      {
        calculatedSum += value.getValue();
      }

      final int numberOfMeasurements = selectedMetric.getMetricValues().size();
      double result = -1;
      if (numberOfMeasurements > 0) {
      result = calculatedSum / numberOfMeasurements;
      }

     return result;
   }
   catch(Exception e)
   {
     return 0.0;
   }
 }

  public double getMaxForMetric(final String metricKey)
  {
    final MetricData selectedMetric = getMetricByKey(metricKey);

    double max = Long.MIN_VALUE;
    for (MetricValues value : selectedMetric.getMetricValues()) {
      max = Math.max(value.getMax(), max);
    }
    return max;
  }

  public double getMinForMetric(final String metricKey) {
    final MetricData selectedMetric = getMetricByKey(metricKey);

    double min = Double.MAX_VALUE;
    for (MetricValues value : selectedMetric.getMetricValues()) {
      min = Math.min(value.getMin(), min);
    }
    return min;
  }

  public String getName() 
  {
    
   // DateTimeFormatter dateTimeFormat = DateTimeFormat.mediumDateTime();
    String description = getBuild().getDescription();
    String name = "";
    
    if(description != null && !"".equals(description.trim()))
    {
          name = String.format("NetStorm Performance Report ( " + getBuild().getDescription() + " ) for time %s - with a duration of %s",
           new SimpleDateFormat("MM/dd/yyyy HH:mm:ss").format(new Date(this.reportTimestamp)), getReportDurationInMinutes());
    
    }
    else
    {
       if(!isNDE)    	
    	  name = String.format("NetStorm Performance Report for time %s - with a duration of %s" ,
    			new SimpleDateFormat("MM/dd/yyyy HH:mm:ss").format(new Date(this.reportTimestamp)), getReportDurationInMinutes());
       else
          name = String.format("NetDiagnostics Performance Report for time %s - with a duration of %s",
 		      new SimpleDateFormat("MM/dd/yyyy HH:mm:ss").format(new Date(this.reportTimestamp)), reportDurationInMinutes);
 
    	
    }
    
    logger.log(Level.INFO, "get name for report - "+ name);
    
    return name;
  }
  
  public String getNameND() 
  {
    String name = "";
    
     name = String.format("NetDiagnostics Performance Report for time %s ", getTestReportND().getCurrentDateTime());
 
     logger.log(Level.INFO, "get name for report in Integrated - "+ name);
      return name;
  }

  public long getTimestamp() {
    return reportTimestamp;
  }

  public Run<?, ?> getBuild() {
    return buildAction.getBuild();
  }

  NetStormBuildAction getBuildAction() {
    return buildAction;
  }

  void setBuildAction(NetStormBuildAction buildAction) {
    this.buildAction = buildAction;
  }

  public void setLastBuildReport(NetStormReport lastBuildReport) {
    this.lastBuildReport = lastBuildReport;
  }

  public String getCustomHTMLReport() {
    return customHTMLReport;
  }

  public void setCustomHTMLReport(String customHTMLReport) {
    try
    {
      if(!customHTMLReport.equals("NA"))
        this.customHTMLReport = URLDecoder.decode(customHTMLReport, "UTF-8");
    }
    catch(Exception e)
    {
    	logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
    }
  }
  
  
  public boolean isIntegrated() {
	return isIntegrated;
  }

  public void setIsIntegrated(boolean isIntegrated) {
	this.isIntegrated = isIntegrated;
  }

   public String getReportDurationInMinutes() {
	return reportDurationInMinutes;
  }

  public void setReportDurationInMinutes(String reportDurationInMinutes) {
	this.reportDurationInMinutes = reportDurationInMinutes;
  }
}
