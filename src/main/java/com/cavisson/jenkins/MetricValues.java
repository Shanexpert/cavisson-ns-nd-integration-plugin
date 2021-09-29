/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.text.SimpleDateFormat;
import java.util.Date;


/**
 *
 * @author preety.yadav
 */
public class MetricValues {
    //private static final DateTimeFormatter DATE_TIME_FORMATTER = DateTimeFormat.mediumDateTime();

  private double current;

  @Override
  public String toString()
  {
    return "MetricValues{" + "current=" + current + ", max=" + max + ", min=" + min + ", startTimeInMillis=" + startTimeInMillis + ", value=" + value + '}';
  }
  private double max;
  private double min;
  private Long startTimeInMillis;
  private double value;

  public double getCurrent() {
    return current;
  }

  public void setCurrent(Double current) {
    this.current = current;
  }

  public double getMax() {
    return max;
  }

  public void setMax(Double max) {
    this.max = max;
  }

  public double getMin() {
    return min;
  }

  public void setMin(Double min) {
    this.min = min;
  }

  public Long getStartTimeInMillis() {
    return startTimeInMillis;
  }

  public void setStartTimeInMillis(Long startTimeInMillis) {
    this.startTimeInMillis = startTimeInMillis;
  }

  public String getFormattedTime() {
    return new SimpleDateFormat("MM/dd/yyyy HH:mm:ss").format(new Date(startTimeInMillis));
  }

  public double getValue() {
    return value;
  }

  public void setValue(Double value) {
    this.value = value;
  }
    
    
}
