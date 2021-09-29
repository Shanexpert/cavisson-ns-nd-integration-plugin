package com.cavisson.jenkins;

import java.util.ArrayList;
import java.util.HashMap;

public class MetricInfo {

	private String groupName;
	private ArrayList<ScalarVal> groupInfo;
	private ArrayList<String> vectorList;
	private ArrayList<MetricVal> vectorObj;
        private ArrayList<MetricLinkInfo> metricLink;

        public ArrayList<MetricLinkInfo> getMetricLink() {
		return metricLink;
	}
	public void setMetricLink(ArrayList<MetricLinkInfo> metricLink) {
		this.metricLink = metricLink;
	}	
	
	public String getGroupName() {
		return groupName;
	}
	public void setGroupName(String groupName) {
		this.groupName = groupName;
	}
	public ArrayList<ScalarVal> getGroupInfo() {
		return groupInfo;
	}
	public void setGroupInfo(ArrayList<ScalarVal> groupInfo) {
		this.groupInfo = groupInfo;
	}
	public ArrayList<String> getVectorList() {
		return vectorList;
	}
	public void setVectorList(ArrayList<String> vectorList) {
		this.vectorList = vectorList;
	}
	public ArrayList<MetricVal> getVectorObj() {
		return vectorObj;
	}
	public void setVectorObj(ArrayList<MetricVal> vectorObj) {
		this.vectorObj = vectorObj;
	}
	
	
	
}
