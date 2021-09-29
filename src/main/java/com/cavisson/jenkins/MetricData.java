/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.util.ArrayList;
import java.util.List;

/**
 *
 * @author preety.yadav
 */
public class MetricData {
     private String frequency;

  @Override
  public String toString()
  {
    return "MetricData{" + "frequency=" + frequency + ", metricPath=" + metricPath + ", metricValues=" + metricValues + '}';
  }
  
  private String metricPath;
  private List<MetricValues> metricValues = new ArrayList<MetricValues>();


  public String getFrequency() {
    return frequency;
  }

  public void setFrequency(String frequency) {
    this.frequency = frequency;
  }

  public String getMetricPath() {
    return metricPath;
  }

  public void setMetricPath(String metricPath) {
    this.metricPath = metricPath;
  }

  public List<MetricValues> getMetricValues() {
    return metricValues;
  }

  public void setMetricValues(List<MetricValues> metricValues) {
    this.metricValues = metricValues;
  }
}
