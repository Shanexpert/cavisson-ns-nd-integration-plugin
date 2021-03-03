/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.util.ArrayList;
import java.util.HashMap;

import com.cavisson.jenkins.PageDetail;

/**
 *
 * @author preety.yadav
 */
public class TestReport {
     private  String overAllStatus;

  @Override
  public String toString()
  {
    return "TestReport{" + "overAllStatus=" + overAllStatus + ", date=" + date + ", overAllFailCriteria=" + overAllFailCriteria + ", testMetrics=" + testMetrics + ", serverName=" + serverName + ", previousTestRun=" + previousTestRun + ", baseLineTestRun=" + baseLineTestRun + ", baselineDateTime=" + baselineDateTime + ", currentDateTime=" + currentDateTime + ", initialTestRun=" + initialTestRun + ", initialDateTime=" + initialDateTime + ", complete=" + complete + ", success=" + success + ", totalTimeOut=" + totalTimeOut + ", cvFail=" + cvFail + ", t4xx=" + t4xx +'}';
  }
  
  private String date;
  private String overAllFailCriteria; 
  private ArrayList<TestMetrics> testMetrics;
  private String serverName;
  private String previousTestRun;
  private String baseLineTestRun;
  private String baselineDateTime;
  private String previousDateTime;
  private String currentDateTime;
  private String initialTestRun;
  private String initialDateTime;
  private String testRun ;
  private String normalThreshold;
  private String criticalThreshold;
  private String operator; 
  private String complete;
  private String success;
  private String totalTimeOut;
  private String cvFail;
  private String conFail;
  private String t4xx;
  private String t5xx;
  private String warVersion;
  private String previousDescription;
  private String baselineDescription;
  private String initialDescription;
  private String currentDescription;
  private String releaseVersion;
  private boolean showHideTransaction = false;
  private String ipPortLabel;
  private String userName;
  private String reportLink;
  private String dashboardURL;  
  private ArrayList<ScalarVal> transactionStats;
  private ArrayList<ScalarVal> scalarGroups;
  private ArrayList<String> scalarHeaders;
  /* ArrayList for setting the vectors list for transaction stats table */
  private ArrayList<String> vecList;
  private ArrayList<MetricVal> metricValues;
  private ArrayList<MetricInfo> vectorValues;
 private ArrayList<MetricLinkInfo> transMetricLink;  //for transaction metric link
 private String showCount;
 private ArrayList<String> pageDetailHeader;
 
 public ArrayList<String> getPageDetailHeader() {
	return pageDetailHeader;
}

public void setPageDetailHeader(ArrayList<String> pageDetailHeader) {
	this.pageDetailHeader = pageDetailHeader;
}

private ArrayList<PageDetail> pageDetail; 

  
 
public ArrayList<PageDetail> getPageDetail() {
	return pageDetail;
}

public void setPageDetail(ArrayList<PageDetail> pageDetail) {
	this.pageDetail = pageDetail;
}

  
public ArrayList<MetricLinkInfo> getTransMetricLink() {
	return transMetricLink;
}

public void setTransMetricLink(ArrayList<MetricLinkInfo> transMetricLink) {
	this.transMetricLink = transMetricLink;
}

public ArrayList<MetricInfo> getVectorValues() {
	return vectorValues;
}

public void setVectorValues(ArrayList<MetricInfo> vectorValues) {
	this.vectorValues = vectorValues;
}

public ArrayList<MetricVal> getMetricValues() {
	return metricValues;
}

public void setMetricValues(ArrayList<MetricVal> metricValues) {
	this.metricValues = metricValues;
}

public ArrayList<String> getVecList() {
	return vecList;
}

public void setVecList(ArrayList<String> vecList) {
	this.vecList = vecList;
}

public ArrayList<ScalarVal> getScalarGroups() {
	return scalarGroups;
}

public void setScalarGroups(ArrayList<ScalarVal> scalarGroups) {
	this.scalarGroups = scalarGroups;
}

public ArrayList<String> getScalarHeaders() {
	return scalarHeaders;
}

public void setScalarHeaders(ArrayList<String> scalarHeaders) {
	this.scalarHeaders = scalarHeaders;
}

public ArrayList<ScalarVal> getTransactionStats() {
	return transactionStats;
}

public void setTransactionStats(ArrayList<ScalarVal> transactionStats) {
	this.transactionStats = transactionStats;
}

public String getDashboardURL() {
       return dashboardURL;
}

public void setDashboardURL(String dashboardURL) {
        this.dashboardURL = dashboardURL;
}

public String getReportLink() {
       return reportLink;
}

public void setReportLink(String reportLink) {
        this.reportLink = reportLink;
}

public String getUserName() {
       return userName;
}

public void setUserName(String userName) {
        this.userName = userName;
}

public String getIpPortLabel() {
	return ipPortLabel;
}

public void setIpPortLabel(String ipPortLabel) {
        this.ipPortLabel = ipPortLabel;
}

public boolean isShowHideTransaction() {
	return showHideTransaction;
}

public void setShowHideTransaction(boolean showHideTransaction) {
	this.showHideTransaction = showHideTransaction;
}

private ArrayList<TransactionStats> transStatsList = new ArrayList<TransactionStats>();
  
  public String getTestRun()
  {
    return testRun;
  }

  public void setTestRun(String testRun)
  {
   this.testRun = testRun;
  }

   public ArrayList<TransactionStats> getTransStatsList()
   {
     return transStatsList;
   }
    
  public String getNormalThreshold()
  {
    return normalThreshold;
  }

  public void setNormalThreshold(String normalThreshold)
  {
    this.normalThreshold = normalThreshold;
  }

  public String getCriticalThreshold()
  {
    return " " +criticalThreshold;

  }

  public void setCriticalThreshold(String criticalThreshold)
  {
    this.criticalThreshold = criticalThreshold;
  }

  public String getOverAllStatus()
  {
    return overAllStatus;
  }

  public void setOverAllStatus(String overAllStatus)
  {
    this.overAllStatus = overAllStatus;
  }

  public String getDate()
  {
    return date;
  }

  public void setDate(String date)
  {
    this.date = date;
  }

  public String getOverAllFailCriteria()
  {
    return overAllFailCriteria;
  }

  public void setOverAllFailCriteria(String overAllFailCriteria)
  {
    this.overAllFailCriteria = overAllFailCriteria;
  }

  public ArrayList<TestMetrics> getTestMetrics()
  {
    return testMetrics;
  }

  public void setTestMetrics(ArrayList<TestMetrics> testMetrics)
  {
    this.testMetrics = testMetrics;
  }

  public String getServerName()
  {
    if(serverName.contains("NetStorm IP:"))
      serverName = serverName.substring(serverName.lastIndexOf(" ") + 1); 

    return serverName;
  }

  public void setServerName(String serverName)
  {
    this.serverName = serverName;
  }

  public String getPreviousTestRun()
  {
    return previousTestRun;
  }

  public void setPreviousTestRun(String previousTestRun)
  {
    this.previousTestRun = previousTestRun;
  }

  public String getBaseLineTestRun()
  {
    return baseLineTestRun;
  }

  public void setBaseLineTestRun(String baseLineTestRun)
  {
    this.baseLineTestRun = baseLineTestRun;
  }  
  
  public void setOperator(String operator)
  {
    this.operator = operator;
  }
  
  public String getOperator()
  {
    return operator;
  }

  public String getBaselineDateTime() 
  {
    return baselineDateTime;
  }

  public void setBaselineDateTime(String baselineDateTime)
  {
    this.baselineDateTime = baselineDateTime;
  }

  public String getPreviousDateTime()
  {
    return previousDateTime;
  }

  public void setPreviousDateTime(String previousDateTime)
  {
    this.previousDateTime = previousDateTime;
  }
  
  public String getInitialDateTime()
  {
    return initialDateTime;
  }

  public void setInitialDateTime(String initialDateTime)
  {
    this.initialDateTime = initialDateTime;
  }
  
  public String getCurrentDateTime()
  {
    return currentDateTime;
  }
  
  public void setCurrentDateTime(String currentDateTime)
  {
    this.currentDateTime = currentDateTime;
  }

  public String getInitialTestRun()
  {
    return initialTestRun;
  }

  public void setInitialTestRun(String initialTestRun)
  {
    this.initialTestRun = initialTestRun;
  }
  
  public String getComplete() {
  return complete;
}

public void setComplete(String complete) {
  this.complete = complete;
}
 
public String getSuccess() {
  return success;
}
 
public void setSuccess(String success) {
  this.success = success;
}
 
public String getTotalTimeOut() {
  return totalTimeOut;
}
 
public void setTotalTimeOut(String totalTimeOut) {
  this.totalTimeOut = totalTimeOut;
}

public String getCvFail() {
  return cvFail;
}
 
public void setCvFail(String cvFail) {
  this.cvFail = cvFail;
}
 
public String getConFail() {
  return conFail;
}
 
public void setConFail(String conFail) {
  this.conFail = conFail;
}
 
public String getT4xx() {
  return t4xx;
}
 
public void setT4xx(String t4xx) {
  this.t4xx = t4xx;
}
 
public String getT5xx() {
  return t5xx;
}
 
public void setT5xx(String t5xx) {
  this.t5xx = t5xx;
}

public String getWarVersion()
{
    return warVersion;
}

public void setWarVersion(String warVersion) 
{
    this.warVersion = warVersion;
}

public String getReleaseVersion()
{
    return releaseVersion;
}

public void setReleaseVersion(String releaseVersion)
{
    this.releaseVersion = releaseVersion;
}

public void setPreviousDescription(String previousDescription)
{
   this.previousDescription = previousDescription;
}

public String getPreviousDescription()
{
   return previousDescription;
}

public void setBaselineDescription(String baselineDescription)
{
   this.baselineDescription = baselineDescription;
}

public String getBaselineDescription()
{
   return baselineDescription;
}

public void setInitialDescription(String initialDescription)
{
   this.initialDescription = initialDescription;
}

public String getInitialDescription()
{
   return initialDescription;
}

public void setCurrentDescription(String currentDescription)
{
   this.currentDescription = currentDescription;
}

public String getCurrentDescription()
{
   return currentDescription;
}

public String getShowCount() {
	return showCount;
}

public void setShowCount(String showCount) {
	this.showCount = showCount;
}

    
}
