package com.cavisson.jenkins;

public class NetDiagnosticsParamtersForReport {
  
	
  private String curStartTime;
  private String curEndTime;
  private String base1StartTime;
  private String base1EndTime;
  private String base2StartTime;
  private String base2EndTime;
  private boolean prevDuration;
  private String base3StartTime;
  private String base3EndTime;
  private String checkProfilePath;
  private String critiThreshold;
  private String warThreshold;
  private String failThreshold;
  private String base1MSRName;
  private String base2MSRName;
  private String base3MSRName;

  
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
public String getBase1StartTime() {
	return base1StartTime;
}
public void setBase1StartTime(String base1StartTime) {
	this.base1StartTime = base1StartTime;
}
public String getBase1EndTime() {
	return base1EndTime;
}
public void setBase1EndTime(String base1EndTime) {
	this.base1EndTime = base1EndTime;
}
public String getBase2StartTime() {
	return base2StartTime;
}
public void setBase2StartTime(String base2StartTime) {
	this.base2StartTime = base2StartTime;
}
public String getBase2EndTime() {
	return base2EndTime;
}
public void setBase2EndTime(String base2EndTime) {
	this.base2EndTime = base2EndTime;
}
public String getBase3StartTime() {
	return base3StartTime;
}
public void setBase3StartTime(String base3StartTime) {
	this.base3StartTime = base3StartTime;
}
public String getBase3EndTime() {
	return base3EndTime;
}
public void setBase3EndTime(String base3EndTime) {
	this.base3EndTime = base3EndTime;
}
public String getBase1MSRName() {
	return base1MSRName;
}
public void setBase1MSRName(String base1msrName) {
	base1MSRName = base1msrName;
}
public String getBase2MSRName() {
	return base2MSRName;
}
public void setBase2MSRName(String base2msrName) {
	base2MSRName = base2msrName;
}
public String getBase3MSRName() {
	return base3MSRName;
}
public void setBase3MSRName(String base3msrName) {
	base3MSRName = base3msrName;
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
	return "curStartTime=" + curStartTime + ",curEndTime=" + curEndTime+ ",base1StartTime=" + base1StartTime + ",base1EndTime=" + base1EndTime + ",base2StartTime=" + base2StartTime + ", base2EndTime =" + base2EndTime + ", base3StartTime=" + base3StartTime + ", base3EndTime=" + base3EndTime + ", base1MSRName=" + base1MSRName+", base2MSRName=" + base2MSRName + ", base3MSRName=" + base3MSRName+",checkProfilePath="+ checkProfilePath +",criThreshold="+critiThreshold +",warThreshold="+warThreshold +",failThreshold="+failThreshold;
}
  
  

	
}
