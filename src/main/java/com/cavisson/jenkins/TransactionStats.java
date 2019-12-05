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
public class TransactionStats {
    
    private String cvFail;
  private String conFail;
  private String t4xx;
  private String t5xx;
  private String warVersion = "NA";
  private String releaseVersion = "NA";
  private String complete;
  private String success;
  private String totalTimeOut;
  private String transTestRun;
  private boolean isNDE = false;
  
  @Override
  public String toString() {
      return "TransactionStats{" + "cvFail=" + cvFail + ", conFail=" + conFail + ", t4xx=" + t4xx + ", t5xx=" + t5xx + ", warVersion=" + warVersion + ", releaseVersion=" + releaseVersion + ", complete=" + complete + ", success=" + success + ", totalTimeOut=" + totalTimeOut + ", transTestRun=" + transTestRun + ", isNDE=" + isNDE + '}';
  }
 
  
  public boolean isNDE() {
	return isNDE;
}

public void setNDE(boolean isNDE) {
	this.isNDE = isNDE;
}



public String getWarVersion() {
     return warVersion;
  }

  public void setWarVersion(String warVersion) {
      this.warVersion = warVersion;
  }

  public String getReleaseVersion() {
     return releaseVersion;
  }

  public void setReleaseVersion(String releaseVersion) {
      this.releaseVersion = releaseVersion;
  }


  public String getTransTestRun() {
      return transTestRun;
  }

  public void setTransTestRun(String transTestRun) {
      this.transTestRun = transTestRun;
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
  
    
}
