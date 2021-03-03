/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

/**
 *
 * @author preety.yadav
 */
public class TestMetrics {
    
  private String prevTestRunValue;
  private String baseLineValue;
  private String initialValue;
  private String currentDateTime;
  private String currValue;
  private String metricName;
  private String operator;
  private String metricRuleName;
  private String transactiontStatus;
  private String statusColour;
  private String transactionTooltip;
  private String linkForTrend;
  private String edLink;
  private String displayName;
  private String headerName;
  private String newReport;
  private String metricLink;
  private String count;

  @Override
  public String toString()
  {
    return "TestMetrics{" + "prevTestRunValue=" + prevTestRunValue + ", baseLineValue=" + baseLineValue +", currentDateTime" +currentDateTime +", currValue=" + currValue + ", metricName=" + metricName + ", operator=" + operator + ", SLA=" + SLA + ", metricRuleName="+ metricRuleName + ", transactiontStatus="+ transactiontStatus + ", statusColour = " + statusColour + ", trendLink = " + linkForTrend + ", count = " + count + "}";
  }
  private String SLA;

  public String getCount() {
	  return count;
  }

  public void setCount(String count) {
	  this.count = count;
  }
  
  public String getEdLink() {
    return edLink;
  }

  public void setEdLink(String edLink) {
    this.edLink = edLink;
  }

  public String getPrevTestRunValue()
  {
    return prevTestRunValue;
  }

  public void setPrevTestRunValue(String prevTestRunValue)
  {
    this.prevTestRunValue = prevTestRunValue;
  }

  public String getBaseLineValue()
  {
    return baseLineValue;
  }

  public void setBaseLineValue(String baseLineValue)
  {
    this.baseLineValue = baseLineValue;
  }

  public String getCurrValue()
  {
    return currValue;
  }

  public void setCurrValue(String currValue)
  {
    this.currValue = currValue;
  }

  public String getMetricName()
  {
    return metricName;
  }

  public void setMetricName(String metricName)
  {
    this.metricName = metricName;
  }

  public String getMetricRuleName()
  {
    return metricRuleName;
  }

  public void setMetricRuleName(String metricRuleName)
  {
    this.metricRuleName = metricRuleName;
  }


  public String getOperator()
  {
    return operator;
  }

  public void setOperator(String operator)
  {
    this.operator = operator;
  }

  public String getSLA()
  {
    return SLA;
  }

  public void setSLA(String SLA)
  {
    this.SLA = SLA;
  }

  public String getInitialValue() 
  {
    return initialValue;
  }

  public void setInitialValue(String initialValue)
  {
    this.initialValue = initialValue;
  }

  public String getCurrentDateTime() 
  {
    return currentDateTime;
  }

  public void setCurrentDateTime(String currentDateTime)
  {
    this.currentDateTime = currentDateTime;
  }
  
  public String getTransactiontStatus()
  {
    return transactiontStatus;
  }

  public void setTransactiontStatus(String transactiontStatus)
  {
    this.transactiontStatus = transactiontStatus;
  }

  public String getStatusColour()
  {
    return statusColour;
  }

  public void setStatusColour(String statusColour)
  {
    this.statusColour = statusColour;
  }

  public void setTransactionTooltip(String transactionTooltip) 
  {
    this.transactionTooltip = transactionTooltip;
  }

  public String getTransactionTooltip() 
  {
    return transactionTooltip;
  }
     
  public void setLinkForTrend(String linkForTrend) 
  {
    this.linkForTrend = linkForTrend;
  }

  public String getLinkForTrend() 
  {
    return linkForTrend;
  }

  public void setDisplayName(String displayName)
  {
    this.displayName = displayName;
  }

  public String getDisplayName()
  {
    return displayName;
  }
  
  
  public void setHeaderName(String headerName)
  {
    this.headerName = headerName;
  }

  public String getHeaderName()
  {
    return headerName;
  }
  
  public void setNewReport(String newReport)
  {
    this.newReport = newReport;
  }

  public String getNewReport()
  {
    return newReport;
  }

   public void setMetricLink(String metricLink)
  {
    this.metricLink = metricLink;
  }

  public String getMetricLink()
  {
    return metricLink;
  } 
  
  
}
