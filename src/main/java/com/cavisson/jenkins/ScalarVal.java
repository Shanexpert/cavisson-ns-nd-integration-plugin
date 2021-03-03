package com.cavisson.jenkins;

import java.util.HashMap;

public class ScalarVal {

	private String Operator;
	private String prevTestValue; 
	private String Prod;
	private String baselineValue; 
	private String transactionStatus;
	private String transactionTooltip;
	private String transactionBGcolor;
	private String Value;
	private String link;
	private String SLA;
	private String initialValue; 
	private String Stress;
	private String MetricName;
	private String VectorName;
	private String trendLink;
	
	
	
	public String getVectorName() {
		return VectorName;
	}
	public void setVectorName(String vectorName) {
		VectorName = vectorName;
	}
	public String getMetricName() {
		return MetricName;
	}
	public void setMetricName(String metricName) {
		MetricName = metricName;
	}
	public String getOperator() {
		return Operator;
	}
	public void setOperator(String operator) {
		Operator = operator;
	}
	public String getPrevTestValue() {
		return prevTestValue;
	}
	public void setPrevTestValue(String prevTestValue) {
		this.prevTestValue = prevTestValue;
	}
	public String getProd() {
		return Prod;
	}
	public void setProd(String prod) {
		Prod = prod;
	}
	public String getBaselineValue() {
		return baselineValue;
	}
	public void setBaselineValue(String baselineValue) {
		this.baselineValue = baselineValue;
	}
	public String getTransactionStatus() {
		return transactionStatus;
	}
	public void setTransactionStatus(String transactionStatus) {
		this.transactionStatus = transactionStatus;
	}
	public String getTransactionTooltip() {
		return transactionTooltip;
	}
	public void setTransactionTooltip(String transactionTooltip) {
		this.transactionTooltip = transactionTooltip;
	}
	public String getTransactionBGcolor() {
		return transactionBGcolor;
	}
	public void setTransactionBGcolor(String transactionBGcolor) {
		this.transactionBGcolor = transactionBGcolor;
	}
	public String getValue() {
		return Value;
	}
	public void setValue(String value) {
		Value = value;
	}
	public String getLink() {
		return link;
	}
	public void setLink(String link) {
		this.link = link;
	}
	public String getSLA() {
		return SLA;
	}
	public void setSLA(String sLA) {
		SLA = sLA;
	}
	public String getInitialValue() {
		return initialValue;
	}
	public void setInitialValue(String initialValue) {
		this.initialValue = initialValue;
	}
	public String getStress() {
		return Stress;
	}
	public void setStress(String stress) {
		Stress = stress;
	}
	public String getTrendLink() {
		return trendLink;
	}
	public void setTrendLink(String trendLink) {
		this.trendLink = trendLink;
	}
	
}
