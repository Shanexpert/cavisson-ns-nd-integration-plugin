/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import hudson.util.FormValidation;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;

import hudson.util.Secret;

/**
 *
 * @author preety.yadav
 */
class FieldValidator {
    
    
     public static FormValidation validateURLConnectionString(final String URLConnectionString) {
        FormValidation validationResult;

        if (isEmptyField(URLConnectionString)) {
            validationResult = FormValidation.error("URL connection string cannot be empty and should start with http:// or https://");
        } else if (!(URLConnectionString.startsWith("http://") || URLConnectionString.startsWith("https://"))) {
            validationResult = FormValidation.error("URL connection should start with http:// or https://");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }

    public static FormValidation validateUsername(final String username) {
        FormValidation validationResult;

        if (isEmptyField(username)) {
            validationResult = FormValidation.error("Username shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }
    
    public static FormValidation validateThresholdValues(final String thresholdValue) {
    		
      FormValidation validationResult;
      String regex = "^[0-9]{2}$";
      
      if (isEmptyField(thresholdValue)) {
          validationResult = FormValidation.error("Threshold value shouldn't be empty");
      } 
      else if(!thresholdValue.matches(regex)){
      		validationResult = FormValidation.error("Please enter correct value.");
      }
      else {
          validationResult = FormValidation.ok();
      }

      return validationResult;
  }
    
  public static FormValidation validateDateTime(final String dateTime) throws ParseException {
    		
      FormValidation validationResult;
      SimpleDateFormat sdf = new SimpleDateFormat("MM/dd/yyyy HH:mm:ss");
      Date testDate = null;
      testDate = sdf.parse(dateTime);
      
      
      if (isEmptyField(dateTime)) {
          validationResult = FormValidation.error("Baseline date time shouldn't be empty");
      } 
      else if(!sdf.format(testDate).equals(dateTime)){
      		validationResult = FormValidation.error("Please enter correct format i.e. MM/dd/yyyy HH:mm:ss");
      }
      else {
          validationResult = FormValidation.ok();
      }

      return validationResult;
  }
    

    public static FormValidation validatePassword(String password) {
        FormValidation validationResult;

        if (isEmptyField(password)) {
            validationResult = FormValidation.error("Password shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }

    public static FormValidation validateProject(final String projectName) {
        FormValidation validationResult;

        if (isEmptyField(projectName)) {
            validationResult = FormValidation.error("Project name shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }

    public static FormValidation validateSubProjectName(final String subprojectName) {
        FormValidation validationResult;

        if (isEmptyField(subprojectName)) {
            validationResult = FormValidation.error("Sub Project name shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }

    public static FormValidation validateScenario(final String scenario) {
        FormValidation validationResult;

        if (isEmptyField(scenario)) {
            validationResult = FormValidation.error("Scenario shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }

    public static FormValidation validateHtmlTablePath(final String htmlTablePath) {
        FormValidation validationResult;
        
        if(isEmptyField(htmlTablePath)) {
           validationResult = FormValidation.error("Report path shouldn't be empty");
        } else {
            validationResult = FormValidation.ok();
        }

        return validationResult;
    }
    
    public static boolean isEmptyField(final String field) {

        return field == null || field.trim().length() <= 0;
    }

    
}
