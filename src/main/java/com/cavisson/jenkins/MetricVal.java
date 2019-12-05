package com.cavisson.jenkins;

import java.util.ArrayList;
import java.util.HashMap;

public class MetricVal {

   ArrayList<String> headersForTrans;
   String nameOfMetric;
   int countForBenchmark;
   int countForMetrices;
   boolean prod = false;
   boolean stress = false;
   boolean trans = false;
   
   
   
   
   public boolean isTrans() {
	return trans;
   }
   public void setTrans(boolean trans) {
 	this.trans = trans;
   }
   public boolean isProd() {
	return prod;
   }
   public void setProd(boolean prod) {
	this.prod = prod;
   }
   public boolean isStress() {
	return stress;
   }
   public void setStress(boolean stress) {
	this.stress = stress;
   }
   public ArrayList<String> getHeadersForTrans() {
	return headersForTrans;
   }
   public void setHeadersForTrans(ArrayList<String> headersForTrans) {
	this.headersForTrans = headersForTrans;
   }
   public String getNameOfMetric() {
	return nameOfMetric;
   }
   public void setNameOfMetric(String nameOfMetric) {
	this.nameOfMetric = nameOfMetric;
   }
   public int getCountForBenchmark() {
	return countForBenchmark;
   }
   public void setCountForBenchmark(int countForBenchmark) {
	this.countForBenchmark = countForBenchmark;
   }
   public int getCountForMetrices() {
	return countForMetrices;
   }
   public void setCountForMetrices(int countForMetrices) {
	this.countForMetrices = countForMetrices;
   }
   
   
}
