/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import hudson.model.Run;
import hudson.model.ModelObject;
import hudson.model.TaskListener;
import hudson.util.Graph;
import java.io.IOException;
import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.util.Date;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jfree.chart.ChartFactory;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.axis.DateAxis;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.util.RelativeDateFormat;
import org.jfree.data.time.Millisecond;
import org.jfree.data.time.TimeSeries;
import org.jfree.data.time.TimeSeriesCollection;
import org.jfree.data.xy.XYDataset;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.StaplerResponse;


public class BuildActionResultsDisplay implements ModelObject{
    
      private transient static final Logger logger = Logger.getLogger(BuildActionResultsDisplay.class.getName());
    /**
   * The {@link NetStormBuildAction} that this report belongs to.
   */
  private transient NetStormBuildAction buildAction;
  private static Run<?, ?> currentBuild = null;
  private NetStormReport currentReport;
  private boolean isNDE = false;

   /**
   * Parses the reports and build a {@link BuildActionResultsDisplay}.
   *
   * @throws java.io.IOException If a report fails to parse.
   */
  BuildActionResultsDisplay(final NetStormBuildAction buildAction, TaskListener listener, boolean isNDE)throws IOException 
  {
    this.buildAction = buildAction;
    this.isNDE = isNDE;
    currentReport = this.buildAction.getNetStormReport();
    currentReport.setBuildAction(buildAction);
    addPreviousBuildReportToExistingReport();
  }

  
    @Override
    public String getDisplayName() {
      if(isNDE)
        return LocalMessages.ND_REPORT_DISPLAYNAME.toString();
      else
	return LocalMessages.REPORT_DISPLAYNAME.toString();
    }
    
     public Run<?, ?> getBuild()
  {
    return buildAction.getBuild();
  }

  public NetStormReport getNetStormReport() 
  {
    return currentReport;
  }

  private void addPreviousBuildReportToExistingReport() 
  {
    // Avoid parsing all builds.
    if (BuildActionResultsDisplay.currentBuild == null) 
    {
      BuildActionResultsDisplay.currentBuild = getBuild();
    }
    else
    {
      if (BuildActionResultsDisplay.currentBuild != getBuild()) {
        BuildActionResultsDisplay.currentBuild = null;
        return;
      }
    }

    Run<?, ?> previousBuild = getBuild().getPreviousBuild();
    if (previousBuild == null) {
      return;
    }

    NetStormBuildAction previousPerformanceAction = previousBuild.getAction(NetStormBuildAction.class);
    if (previousPerformanceAction == null) {
      return;
    }

    BuildActionResultsDisplay previousBuildActionResults = previousPerformanceAction.getBuildActionResultsDisplay();
    if (previousBuildActionResults == null) {
      return;
    }

    NetStormReport lastReport = previousBuildActionResults.getNetStormReport();
    getNetStormReport().setLastBuildReport(lastReport);
  }
  
  /**
   * Graph of metric points over time.
   */
  public void doSummarizerGraph(final StaplerRequest request, final StaplerResponse response) throws IOException
  {    
    final String metricKey = request.getParameter("metricDataKey");
    final MetricData metricData = this.currentReport.getMetricByKey(metricKey);
    final MetricData metricDataBaseLine = this.currentReport.getMetricByKey(metricKey , "BASELINE");
    final MetricData metricDataPrevious = this.currentReport.getMetricByKey(metricKey , "PREVIOUS");
    
    logger.log(Level.INFO, "Test Report  = " + this.currentReport.getTestReport());
    logger.log(Level.INFO, "Base Line = " + metricDataBaseLine ); 
    logger.log(Level.INFO, "Previous Line = " + metricDataPrevious );
    
    final Date d =  new Date();
    final Graph graph = new GraphImpl(metricKey, metricData.getFrequency()) 
    {      
       protected XYDataset createDataSet()
       {
         TimeSeriesCollection  dataset =   new TimeSeriesCollection();
         TimeSeries  series = new TimeSeries("Current Test", Millisecond.class);
        
         long interval = Integer.parseInt(metricData.getFrequency()) * 1000L;
         
         long time = interval;
         for (MetricValues value : metricData.getMetricValues()) 
         {
            if(getTimeInMillisecondFormat(time , d)  != null)
              series.add(getTimeInMillisecondFormat(time , d), value.getValue() );
            time =  time + interval;
         }
         
         dataset.addSeries(series);
         
         
         if(metricDataBaseLine != null)
         {
            time = interval;
            TimeSeries  series1 = new TimeSeries("Base Line Test", Millisecond.class);
            for (MetricValues value : metricDataBaseLine.getMetricValues()) 
            {
              if(getTimeInMillisecondFormat(time , d)  != null)
                series1.add(getTimeInMillisecondFormat(time , d), value.getValue() );
              time =  time + interval;
           }
            
           dataset.addSeries(series1);
         }
         
         if(metricDataPrevious != null)
         {
           time = interval;
           TimeSeries  series1 = new TimeSeries("Previous Test", Millisecond.class);
           for (MetricValues value : metricDataPrevious.getMetricValues()) 
           {
             if(getTimeInMillisecondFormat(time , d)  != null)
               series1.add(getTimeInMillisecondFormat(time , d), value.getValue() );
             time =  time + interval;
          }
            
           dataset.addSeries(series1);
         }
   
       
  
         return dataset;
        }
    };

    graph.doPng(request, response);
  }

  public Millisecond getTimeInMillisecondFormat(long time, Date startDate)
  {
    try
    {
       Date  dateToday = startDate;
       Date newDate = new Date((long) (dateToday.getTime() + (time)));
       int msec = (int) (time) % 1000;
       int sec = newDate.getSeconds();
       int minute = newDate.getMinutes();
       int hour = newDate.getHours();
       Millisecond elapsedMSecond = new Millisecond(msec, sec, minute, hour, newDate.getDate(), (newDate.getMonth() + 1), (newDate.getYear() + 1900));
      return (elapsedMSecond);
    }
    catch (Exception e)
    {
      return null;
    }
  }


  private abstract class GraphImpl extends Graph 
  {
    private final String graphTitle;
    private final String xLabel;

    protected GraphImpl(final String metricKey, final String frequency) {
      super(-1, 400, 300); // cannot use timestamp, since ranges may change
      this.graphTitle = stripTitle(metricKey);
      this.xLabel = "Time in " + frequency + " secs";
    }

    private String stripTitle(final String metricKey) {
      return metricKey;
    }

    protected abstract XYDataset createDataSet();

    protected JFreeChart createGraph() {
      final XYDataset dataset = createDataSet();

      final JFreeChart chart = ChartFactory.createTimeSeriesChart(graphTitle, // title
          xLabel, // category axis label
          "", // value axis label
          dataset, // data
         // PlotOrientation.VERTICAL, // orientation
          true, // include legends
          true, // tooltips
          false // urls
      );
      
      XYPlot xyplot = chart.getXYPlot();
      DateAxis axis = (DateAxis) xyplot.getDomainAxis();
        
      RelativeDateFormat rdf    = new RelativeDateFormat(new Date());
      rdf.setShowZeroDays(false);
      NumberFormat numberFormat = new DecimalFormat("0");
      numberFormat.setMinimumIntegerDigits(2);
      rdf.setSecondFormatter(numberFormat);
      rdf.setHourSuffix(":");
      rdf.setMinuteSuffix(":");
      rdf.setSecondSuffix("");
      rdf.setNumberFormat(numberFormat);
      axis.setDateFormatOverride(rdf);
      
      return chart;
    }
  }
    
}
