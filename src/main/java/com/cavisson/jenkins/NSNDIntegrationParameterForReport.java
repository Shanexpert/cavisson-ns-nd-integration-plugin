/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

/**
 *
 * @author richa.garg
 */
public class NSNDIntegrationParameterForReport {
    
  private String curStartTime;
  private String curEndTime;
  private String baseStartTime;
  private String baseEndTime;
  private String initStartTime;
  private String initEndTime;
  private boolean prevDuration;
  private String checkProfilePath;
  private String critiThreshold;
  private String warThreshold;
  private String failThreshold;

  
public String getCritiThreshold() {
	return critiThreshold;
}
public void setCritiThreshold(String critiThreshold) {
	this.critiThreshold = critiThreshold;
}
public String getWarThreshold() {
	return warThreshold;
}
public void setWarThreshold(String warThreshold) {
	this.warThreshold = warThreshold;
}
public String getFailThreshold() {
	return failThreshold;
}
public void setFailThreshold(String failThreshold) {
	this.failThreshold = failThreshold;
}
public String getCurStartTime() {
	return curStartTime;
}
public void setCurStartTime(String curStartTime) {
	this.curStartTime = curStartTime;
}
public String getCurEndTime() {
	return curEndTime;
}
public void setCurEndTime(String curEndTime) {
	this.curEndTime = curEndTime;
}
public String getBaseStartTime() {
	return baseStartTime;
}
public void setBaseStartTime(String baseStartTime) {
	this.baseStartTime = baseStartTime;
}
public String getBaseEndTime() {
	return baseEndTime;
}
public void setBaseEndTime(String baseEndTime) {
	this.baseEndTime = baseEndTime;
}
public String getInitStartTime() {
	return initStartTime;
}
public void setInitStartTime(String initStartTime) {
	this.initStartTime = initStartTime;
}
public String getInitEndTime() {
	return initEndTime;
}
public void setInitEndTime(String initEndTime) {
	this.initEndTime = initEndTime;
}
public boolean isPrevDuration() {
	return prevDuration;
}
public void setPrevDuration(boolean prevDuration) {
	this.prevDuration = prevDuration;
}
public String getCheckProfilePath() {
	return checkProfilePath;
}
public void setCheckProfilePath(String checkProfilePath) {
	this.checkProfilePath = checkProfilePath;
}


@Override
public String toString() {
	return "curStartTime=" + curStartTime + ",curEndTime=" + curEndTime+ ",baseStartTime=" + baseStartTime + ",baseEndTime=" + baseEndTime + ",initStartTime=" + initStartTime+ ",initEndTime=" + initEndTime + ",prevDuration=" + prevDuration + ",checkProfilePath="+ checkProfilePath +",criThreshold="+critiThreshold +",warThreshold="+warThreshold +",failThreshold="+failThreshold;
}
  
}
