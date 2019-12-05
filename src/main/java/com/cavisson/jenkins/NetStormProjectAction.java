/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

//import hudson.model.Run;
import hudson.model.AbstractProject;
import hudson.model.Action;
import hudson.model.Run;
import hudson.util.ChartUtil;
import hudson.util.DataSetBuilder;
import hudson.util.Graph;
import java.awt.Color;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.logging.Logger;
import org.jfree.chart.ChartFactory;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.data.category.CategoryDataset;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.StaplerResponse;

/**
 *
 * @author preety.yadav
 */
public class NetStormProjectAction implements Action  {
    
    private static final Logger LOGGER = Logger.getLogger(NetStormProjectAction.class.getName());
  private static final long serialVersionUID = 1L;

  private static final String PLUGIN_NAME = "netstorm-dashboard";

  private final AbstractProject<?, ?> project;
  private final String mainMetricKey = "Average Transaction Response Time (Secs)";
  private final String[] allMetricKeys;

  public NetStormProjectAction(final AbstractProject project, final String[] allMetricKeys) 
  {
    this.project = project;
    this.allMetricKeys = allMetricKeys;
  }

  public String getDisplayName() 
  {
    return LocalMessages.PROJECTACTION_DISPLAYNAME.toString();
  }

  public String getUrlName() 
  {
    return PLUGIN_NAME;
  }

  public String getIconFileName() 
  {
    return "graph.gif";
  }

  /**
   * Method necessary to get the side-panel included in the Jelly file
   * @return this {@link AbstractProject}
   */
  public AbstractProject getProject() 
  {
    return this.project;
  }

  public boolean isTrendVisibleOnProjectDashboard() 
  {
    return getExistingReportsList().size() >= 1;
  }

  public List<String> getAvailableMetricKeys()
  {
    return Arrays.asList(allMetricKeys);
  }

  /**
   * Graph of metric points over time.
   */
  public void doSummarizerGraphMainMetric(final StaplerRequest request,
                                          final StaplerResponse response) throws IOException {
    final Map<ChartUtil.NumberOnlyBuildLabel, Double> averagesFromReports =
        getAveragesFromAllReports(getExistingReportsList(), mainMetricKey);

    final Graph graph = new GraphImpl(mainMetricKey + " Overall Graph") {

      protected DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel> createDataSet() {
        DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel> dataSetBuilder =
            new DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel>();

        for (ChartUtil.NumberOnlyBuildLabel label : averagesFromReports.keySet()) {
          dataSetBuilder.add(averagesFromReports.get(label), mainMetricKey, label);
        }

        return dataSetBuilder;
      }
    };

    graph.doPng(request, response);
  }


  /**
   * Graph of metric points over time, metric to plot set as request parameter.
   */
  /**
   * Graph of metric points over time, metric to plot set as request parameter.
   */
  public void doSummarizerGraphForMetric(final StaplerRequest request,
                                          final StaplerResponse response) throws IOException {
    final String metricKey = request.getParameter("metricDataKey");
    final Map<ChartUtil.NumberOnlyBuildLabel, Double> averagesFromReports =
        getAveragesFromAllReports(getExistingReportsList(), metricKey);

    final Graph graph = new GraphImpl(metricKey + " Overall") {

      protected DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel> createDataSet() {
        DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel> dataSetBuilder =
            new DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel>();

        for (ChartUtil.NumberOnlyBuildLabel label : averagesFromReports.keySet()) {
          dataSetBuilder.add(averagesFromReports.get(label), metricKey, label);
        }

        return dataSetBuilder;
      }
    };

    graph.doPng(request, response);
  }


 private abstract class GraphImpl extends Graph {
    private final String graphTitle;

    protected GraphImpl(final String metricKey) {
      super(-1, 400, 300); // cannot use timestamp, since ranges may change
      this.graphTitle = stripTitle(metricKey);
    }

    private String stripTitle(final String metricKey) {
      return metricKey.substring(metricKey.lastIndexOf("|") + 1);
    }

    protected abstract DataSetBuilder<String, ChartUtil.NumberOnlyBuildLabel> createDataSet();

    protected JFreeChart createGraph() {
      final CategoryDataset dataset = createDataSet().build();

      final JFreeChart chart = ChartFactory.createLineChart(graphTitle, // title
          "Build Number #", // category axis label
          null, // value axis label
          dataset, // data
          PlotOrientation.VERTICAL, // orientation
          false, // include legend
          true, // tooltips
          false // urls
      );

      chart.setBackgroundPaint(Color.white);

      return chart;
    }
  }

  private List<NetStormReport> getExistingReportsList()
  {
    final List<NetStormReport> adReportList = new ArrayList<NetStormReport>();

    if (null == this.project) 
    {
      return adReportList;
    }

    final List<? extends Run<?, ?>> builds = project.getBuilds();
    //This check is used for take Average of last 10 builds. 
    int endIndex = builds.size()-1;
    if(builds.size() >= 10)
    {
      endIndex = 9;
    }

    for (int i = 0; i <= endIndex; i++)
    {
      Run<?, ?> currentBuild = builds.get(i);
      final NetStormBuildAction performanceBuildAction = currentBuild.getAction(NetStormBuildAction.class);
      if (performanceBuildAction == null) 
      {
        continue;
      }
      
      final NetStormReport report = performanceBuildAction.getBuildActionResultsDisplay().getNetStormReport();
      
      if (report == null) 
      {
        continue;
      }

      adReportList.add(report);
    }

    return adReportList;
  }

  private Map<ChartUtil.NumberOnlyBuildLabel, Double> getAveragesFromAllReports(
      final List<NetStormReport> reports, final String metricKey) {
    Map<ChartUtil.NumberOnlyBuildLabel, Double> averages = new TreeMap<ChartUtil.NumberOnlyBuildLabel, Double>();
    for (NetStormReport report : reports) {
      double value = report.getAverageForMetric(metricKey);
      Run <?,?> build = report.getBuild();
      if (value >= 0) {

        ChartUtil.NumberOnlyBuildLabel label = new ChartUtil.NumberOnlyBuildLabel((Run<?, ?>)build);

        averages.put(label, value);
      }
    }

    return averages;
  }
    
}
