/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.util.ArrayList;

/**
 *
 * @author preety.yadav
 */
public class MetricDataContainer {
    
    private ArrayList<MetricData> metricDataList;
  private ArrayList<MetricData> metricBaseLineDataList;
  private ArrayList<MetricData> metricPreviousDataList;
  

  @Override
  
  public String toString()
  {
    return "MetricDataContainer{" + "metricDataList=" + metricDataList + ", testReport=" + testReport + ", frequency=" + frequency + '}';
  }
  
  private TestReport testReport;
  private TestReport testReportND;
  private int frequency;
  private String customHTMLReport = "NA";

  public ArrayList<MetricData> getMetricDataList()
  {
    return metricDataList;
  }

  public void setMetricDataList(ArrayList<MetricData> metricDataList)
  {
    this.metricDataList = metricDataList;
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

  public int getFrequency()
  {
    return frequency;
  }

  public void setFrequency(int frequency)
  {
    this.frequency = frequency;
  }

  public ArrayList<MetricData> getMetricBaseLineDataList() {
    return metricBaseLineDataList;
  }

  public void setMetricBaseLineDataList(ArrayList<MetricData> metricBaseLineDataList) {
    this.metricBaseLineDataList = metricBaseLineDataList;
  }

  public ArrayList<MetricData> getMetricPreviousDataList() {
    return metricPreviousDataList;
  }

  public void setMetricPreviousDataList(ArrayList<MetricData> metricPreviousDataList) {
    this.metricPreviousDataList = metricPreviousDataList;
  }

  public String getCustomHTMLReport() {
    return customHTMLReport;
  }

  public void setCustomHTMLReport(String customHTMLReport) {
    this.customHTMLReport = customHTMLReport;
  }
  
}
