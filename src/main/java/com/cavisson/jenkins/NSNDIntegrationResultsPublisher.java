/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

package com.cavisson.jenkins;


import hudson.Extension;
import hudson.Launcher;
import hudson.model.*;
import hudson.model.Run;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.BuildStepMonitor;
import hudson.tasks.Builder;
import hudson.tasks.BuildStep;
import hudson.tasks.Publisher;
import hudson.tasks.Recorder;
import hudson.util.FormValidation;
import hudson.util.ListBoxModel;
import hudson.Util;
import hudson.EnvVars;
import hudson.FilePath;
import java.io.IOException;
import java.io.PrintStream;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.Collections;
import jenkins.tasks.SimpleBuildStep;

import net.sf.json.JSONObject;
import net.sf.json.JSONArray;
import net.sf.json.*;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;

import com.cavisson.jenkins.NSNDIntegrationConnectionManager;
import com.cavisson.jenkins.NSNDIntegrationParameterForReport;

import jenkins.model.*;

import org.apache.commons.lang.StringUtils;
import hudson.util.Secret;

/**
 *
 * @author richa.garg
 */
public class NSNDIntegrationResultsPublisher extends Recorder implements SimpleBuildStep {

    private static final String DEFAULT_USERNAME = "netstorm";// Default user name for NetStorm
    private static final String DEFAULT_TEST_METRIC = "Average Transaction Response Time (Secs)";// Dafault Test Metric  
    private static final String fileName = "jenkins_check_rule";
    private transient static final Logger logger = Logger.getLogger(NSNDIntegrationResultsPublisher.class.getName());
  
    @Override
    public BuildStepMonitor getRequiredMonitorService() {
         return BuildStepMonitor.NONE;
    }

    @Override
    public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener listener) throws InterruptedException, IOException {
         Map<String, String> env = run instanceof AbstractBuild ? ((AbstractBuild<?,?>) run).getBuildVariables() : Collections.<String, String>emptyMap();    
     PrintStream logger = listener.getLogger();
   StringBuffer errMsg = new StringBuffer();
   

   String curStart = "";
   String curEnd = " ";
   String path = "";
   String jobName = "";
   String criticalThreshold = "";
   String warningThreshold = "";
   String overallThreshold = "";
   Boolean fileUpload = false;

   Set keyset = env.keySet();

   for(Object key : keyset)
   {
     Object value = env.get(key);
     
     String keyEnv = (String)key;
     
     if(value instanceof String)
     {
        if(keyEnv.startsWith("JENKINS_HOME"))
        {
          path = (String) value;
        }

        if(keyEnv.startsWith("JOB_NAME"))
        {
          jobName = (String) value;
        }

        if(keyEnv.equalsIgnoreCase("criticalThreshold"))
        {
          criticalThreshold = (String) value;
        }

        if(keyEnv.equalsIgnoreCase("warningThreshold"))
        {
          warningThreshold = (String) value;
        }

        if(keyEnv.equalsIgnoreCase("overallThreshold"))
        {
          overallThreshold = (String) value;
        }

        if(keyEnv.equalsIgnoreCase(fileName))
        {
          fileUpload = true;
        }

      }
    }

    JSONObject json = null;
    if(fileUpload)
    {
      File file = new File(path+"/workspace/"+ jobName +"/"+fileName);

      if(file.exists())
      {
        BufferedReader reader = new BufferedReader(new FileReader(file));
        StringBuilder builder = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {

          if(line.contains("GroupName") || line.contains("GraphName") ||line.contains("VectorName") || line.contains("RuleDesc"))
          {
            line = line.trim().replaceAll("\\s", "@@");
          }

          builder.append(line.trim());
        }
        json = (JSONObject) JSONSerializer.toJSON(builder.toString());
      }
    }
   
   //Getting initial duration values
//   if(getInitDurationValues() != null)
//   {
//     String duration = getInitDurationValues();
//     String values[] = duration.split("@");
//	   
//     ndParams.setInitStartTime(values[0]);
//     ndParams.setInitEndTime(values[1]);
//   }
     
   
//  ndParams.setPrevDuration(getPrevDuration());
   
   NSNDIntegrationConnectionManager connection = new NSNDIntegrationConnectionManager (nsIntegrationUri, nsUsername, nsPassword, ndIntegrationUri, ndUsername, ndPassword, ndParams);

   connection.setJkRule(json);
   connection.setCritical(criticalThreshold);
   connection.setWarning(warningThreshold);
   connection.setOverall(overallThreshold); 

   
   Project buildProject = (Project) ((AbstractBuild<?,?>) run).getProject();   
    List<Builder> blist = buildProject.getBuilders();
    String testMode = "N";
        
    for(Builder currentBuilder : blist)
    {
      if(currentBuilder instanceof NetStormBuilder)
      {
         testMode = ((NetStormBuilder)currentBuilder).getTestMode();
         if(testMode.equals("T"))
         {
           connection.setProject(((NetStormBuilder)currentBuilder).getProject());
           connection.setSubProject(((NetStormBuilder)currentBuilder).getSubProject());
           connection.setScenario(((NetStormBuilder)currentBuilder).getScenario());
           
         }
         break;
      }
    }
   
   
   NetStormDataCollector dataCollector = new NetStormDataCollector(connection, run , Integer.parseInt(NetStormBuilder.testRunNumber), "T", true, duration);
   logger.println("data collector object in NSNDIntegration.." + dataCollector.toString()); 
   
   try
   {
     NetStormReport report = dataCollector.createReportFromMeasurements(logger, fp);
     //boolean status = dataCollector.createReportFromMeasurements(logger);
//     NetStormBuildAction buildAction = new NetStormBuildAction(run, report, false, true);
//     
//      run.addAction(buildAction);
      run.setDisplayName(NetStormBuilder.testRunNumber);
      NetStormBuilder.testRunNumber = "-1";
     
     //change status of build depending upon the status of report.
//      TestReport tstRpt =  report.getTestReport();
//      if(tstRpt.getOverAllStatus().equals("FAIL"))
//      run.setResult(Result.FAILURE);
//
//     logger.println("Ready building Integrated  report");
//     List<NetStormReport> previousReportList = getListOfPreviousReports(run, report.getTimestamp());
//     
//     double averageOverTime = calculateAverageBasedOnPreviousReports(previousReportList);
//     logger.println("Calculated average from previous reports for integrated: " + averageOverTime);
//
//     double currentReportAverage = report.getAverageForMetric(DEFAULT_TEST_METRIC);
//     logger.println("Metric for integrated: " + DEFAULT_TEST_METRIC + "% . Build status is: " + ((Run<?,?>) run).getResult());
   }
   catch(Exception e)
   {
     logger.println("Not able to create netstorm report for NSNDIntegrated.may be some configuration issue in running scenario.");
     return ;
   }
   
   
   return ;
               
    }
   
//   public String getInitDurationValues()
//   {
//     if(initDuration != null)
//     {
////       if(initDuration.containsKey("initStartTime"))
////       {
////         initStartTime = (String)initDuration.get("initStartTime");
////         setInitStartTime(initStartTime);
////       }
////       
////       if(initDuration.containsKey("initEndTime"))
////       {
////           initEndTime = (String)initDuration.get("initEndTime");
////           setInitEndTime(initEndTime);
////       }   
//         
//     }
//     
//     if(initStartTime != null && initEndTime != null)
//       return initStartTime+"@"+initEndTime;
//     else
//    	return null;
//   }
   
   public String isTimePeriod(String testTypeName) {
	   logger.log(Level.INFO, "inside time period check..." + testTypeName + ", ndParams.getTimePeriod() = " + ndParams.getTimePeriod());
	    return ndParams.getTimePeriod().equalsIgnoreCase(testTypeName) ? "true" : "";
	}
    
   private double calculateAverageBasedOnPreviousReports(final List<NetStormReport> reports)
   {
     double calculatedSum = 0;
     int numberOfMeasurements = 0;
     for (NetStormReport report : reports) 
     {
       double value = report.getAverageForMetric(DEFAULT_TEST_METRIC);
     
       if (value >= 0)
       {
         calculatedSum += value;
         numberOfMeasurements++;
       }
     }

     double result = -1;
     if (numberOfMeasurements > 0)
     {
       result = calculatedSum / numberOfMeasurements;
     }

     return result;
   }
    
    private List<NetStormReport> getListOfPreviousReports(final Run<?, ?> build, final long currentTimestamp) 
   {
     final List<NetStormReport> previousReports = new ArrayList<NetStormReport>();
     
     final NetStormBuildAction performanceBuildAction = build.getAction(NetStormBuildAction.class);
     
     
     /*
      * Adding current report object in to list.
      */
     previousReports.add(performanceBuildAction.getBuildActionResultsDisplay().getNetStormReport());

     return previousReports;
   }
    
//    public boolean isPrevDuration()
//   {
//    return getPrevDuration();
//   }

    NSNDIntegrationParameterForReport  ndParams = new NSNDIntegrationParameterForReport();
    
    @DataBoundConstructor
   public NSNDIntegrationResultsPublisher(final String nsIntegrationUri, final String nsUsername, String nsPassword, final String ndIntegrationUri, final String ndUsername,
         String ndPassword, final String base1StartTime, final String base1EndTime, 
         final String base2StartTime, final String base2EndTime, final String base3StartTime, final String base3EndTime,
         final String checkProfilePath, final String criThreshold, final String warThreshold, final String failThreshold, 
         final String timePeriod, final String curStartTimeAbsolute, final String curEndTimeAbsolute, final String curStartTimeElapsed, 
         final String curEndTimeElapsed, final String phase, final String base1MSRName, final String base2MSRName, final String base3MSRName)
  {

   
   /*creating json for sending the paramters to get the response json. */
   setNsIntegrationUri(nsIntegrationUri);
   setNsUsername(nsUsername);
   setNsPassword(nsPassword);
   setNdIntegrationUri(ndIntegrationUri);
   setNdUsername(ndUsername);
   setNdPassword(ndPassword);
   setBase1StartTime(base1StartTime);
   setBase1EndTime(base1EndTime);
   setBase2StartTime(base2StartTime);
   setBase2EndTime(base2EndTime);
   setBase3StartTime(base3StartTime);
   setBase3EndTime(base3EndTime);
   setBase3MSRName(base3MSRName);
   setBase2MSRName(base2MSRName);
   setBase1MSRName(base1MSRName);
 
   setCheckProfilePath(checkProfilePath);
   setCriThreshold(criThreshold);
   setWarThreshold(warThreshold);
   setFailThreshold(failThreshold);
//   this.initDuration = initDuration;
//   this.prevDuration = prevDuration;
   ndParams.setBase1StartTime(base1StartTime);
   ndParams.setBase1EndTime(base1EndTime);
   ndParams.setBase2StartTime(base2StartTime);
   ndParams.setBase2EndTime(base2EndTime);
   ndParams.setBase3StartTime(base3StartTime);
   ndParams.setBase3EndTime(base3EndTime);
   ndParams.setBase1MSRName(base1MSRName);
   ndParams.setBase2MSRName(base2MSRName);
   ndParams.setBase3MSRName(base3MSRName);
   ndParams.setCheckProfilePath(checkProfilePath);
    
    if(this.getCriThreshold() != "")
      ndParams.setCritiThreshold(this.getCriThreshold());
    else
      ndParams.setCritiThreshold(criThreshold);

    if(this.getWarThreshold() != "")
      ndParams.setWarThreshold(this.getWarThreshold());
    else
      ndParams.setWarThreshold(warThreshold);

    if(this.getFailThreshold() != "")
      ndParams.setFailThreshold(this.getFailThreshold());
    else
      ndParams.setFailThreshold(failThreshold);
    
    setTimePeriod(timePeriod);
    ndParams.setTimePeriod(timePeriod);

 if(timePeriod != null) {
  if(timePeriod.equals("Absolute Time")) {
  	ndParams.setCurStartTimeAbsolute(curStartTimeAbsolute);
  	ndParams.setCurEndTimeAbsolute(curEndTimeAbsolute);
  	setCurStartTimeAbsolute(curStartTimeAbsolute);
  	setCurEndTimeAbsolute(curEndTimeAbsolute);
  } else if(timePeriod.equals("Elapsed Time")) {
  	ndParams.setCurStartTimeElapsed(curStartTimeElapsed);
  	ndParams.setCurEndTimeElapsed(curEndTimeElapsed);
  	setCurStartTimeElapsed(curStartTimeElapsed);
  	setCurEndTimeElapsed(curEndTimeElapsed);
  } else if(timePeriod.equals("Phase")) {
  	ndParams.setPhase(phase);
  	setPhase(phase);
  }
 }

 }

    
       
  public static class DescriptorImpl extends BuildStepDescriptor<Publisher> {

  //This is used to show post build action item
  @Override
  public String getDisplayName()
  {
    return LocalMessages.NSND_PUBLISHER_DISPLAYNAME.toString();
  }

  @Override
  public boolean isApplicable(Class<? extends AbstractProject> jobType)
  {
    return true;
  }

  public String getDefaultUsername()
  {
      return DEFAULT_USERNAME;
  }

 public String getDefaultTestMetric()
 {
   return DEFAULT_TEST_METRIC;
 }
 
 public FormValidation doCheckNsIntegrationUri(@QueryParameter final String nsIntegrationUri)
 {
   return  FieldValidator.validateURLConnectionString(nsIntegrationUri);
 }
 
 public FormValidation doCheckNsPassword(@QueryParameter String nsPassword)
 {	
   return  FieldValidator.validatePassword(nsPassword);
 }
 
 public FormValidation doCheckNsUsername(@QueryParameter final String nsUsername)
 {
   return  FieldValidator.validateUsername(nsUsername);
 }
 
 public synchronized ListBoxModel doFillPhaseItems()
 {
   ListBoxModel models = new ListBoxModel();
     models.add("Duration");  
     return models;
 }
 
 public FormValidation doCheckNdIntegrationUri(@QueryParameter final String ndIntegrationUri)
 {
   return  FieldValidator.validateURLConnectionString(ndIntegrationUri);
 }
 
 public FormValidation doCheckNdPassword(@QueryParameter String ndPassword)
 {
   return  FieldValidator.validatePassword(ndPassword);
 }
 
 public FormValidation doCheckNdUsername(@QueryParameter final String ndUsername)
 {
   return  FieldValidator.validateUsername(ndUsername);
 }
 
 public FormValidation doCheckWarThreshold(@QueryParameter final String warThreshold) {
   return FieldValidator.validateThresholdValues(warThreshold);
} 
 
 public FormValidation doCheckCriThreshold(@QueryParameter final String criThreshold) {
   return FieldValidator.validateThresholdValues(criThreshold);
} 
  
 
 public FormValidation doCheckFailThreshold(@QueryParameter final String failThreshold) {
   return FieldValidator.validateThresholdValues(failThreshold);
} 
 
 public FormValidation doCheckBaseStartTime(@QueryParameter final String baseStartTime) throws ParseException {
   return FieldValidator.validateDateTime(baseStartTime);
} 
 
 public FormValidation doCheckBaseEndTime(@QueryParameter final String baseEndTime) throws ParseException {
   return FieldValidator.validateDateTime(baseEndTime);
}  
 
 /*
 Need to test connection on given credientials
 */
public FormValidation doTestNsNdIntegratedConnection(@QueryParameter("nsIntegrationUri") final String nsIntegrationUri, @QueryParameter("nsUsername") final String nsUsername, @QueryParameter("nsPassword") String nsPassword, @QueryParameter("ndIntegrationUri") final String ndIntegrationUri, @QueryParameter("ndUsername") final String ndUsername, @QueryParameter("ndPassword") String ndPassword ) 
{ 
  
  FormValidation validationResult;
  
  StringBuffer errMsg = new StringBuffer();
 
  if (Util.fixEmptyAndTrim(nsIntegrationUri) == null || FieldValidator.isEmptyField(nsIntegrationUri))
  {
    return validationResult = FormValidation.error("URL connection string for NS cannot be empty and should start with http:// or https://");
  } 
  else if (!(nsIntegrationUri.startsWith("http://") || nsIntegrationUri.startsWith("https://"))) 
  {
    return validationResult = FormValidation.error("URL connection string should start with http:// or https://");
  }
  
  if(Util.fixEmptyAndTrim(nsUsername) == null || FieldValidator.isEmptyField(nsUsername))
  {
    return validationResult = FormValidation.error("Please enter user name.");
  }

  if(Util.fixEmptyAndTrim(nsPassword) == null || FieldValidator.isEmptyField(nsPassword))
  {
    return validationResult = FormValidation.error("Please enter password.");
  }
  
  if (Util.fixEmptyAndTrim(ndIntegrationUri) == null || FieldValidator.isEmptyField(ndIntegrationUri))
  {
    return validationResult = FormValidation.error("URL connection string for ND cannot be empty and should start with http:// or https://");
  } 
  else if (!(ndIntegrationUri.startsWith("http://") || ndIntegrationUri.startsWith("https://"))) 
  {
    return validationResult = FormValidation.error("URL connection string should start with http:// or https://");
  }
  
  if(Util.fixEmptyAndTrim(ndUsername) == null || FieldValidator.isEmptyField(ndUsername))
  {
    return validationResult = FormValidation.error("Please enter user name.");
  }

  if(Util.fixEmptyAndTrim(ndPassword) == null || FieldValidator.isEmptyField(ndPassword))
  {
    return validationResult = FormValidation.error("Please enter password.");
  }
    
  NSNDIntegrationConnectionManager connection = new NSNDIntegrationConnectionManager(nsIntegrationUri, nsUsername, Secret.fromString(nsPassword), ndIntegrationUri, ndUsername, Secret.fromString(ndPassword), null);
  
  String check = ndIntegrationUri + "@@" + ndUsername +"@@" + ndPassword;
  
  if (!connection.testNDConnection(errMsg, check)) 
  { 
    validationResult = FormValidation.warning("Connection to NSNDIntegration unsuccessful, due to: "+  errMsg);
  }
  else
    validationResult = FormValidation.ok("Connection successful");

  return validationResult;
} 

}
  
@Extension
public static final DescriptorImpl DESCRIPTOR = new DescriptorImpl();
  
  
  @Override
 public BuildStepDescriptor<Publisher> getDescriptor()
 {
    return DESCRIPTOR;
 }
 
 private String nsIntegrationUri = "";
 private String nsUsername = "";
 private Secret nsPassword;
 private String ndIntegrationUri = "";
 private String ndUsername = "";
 private Secret ndPassword;
 //private String prevDuration = new String();
 //private String initDuration = new String();
 private String base1StartTime;
 private String base1EndTime;
 private  String checkProfilePath;
 private String base2StartTime;
 private String base2EndTime;
 private String base3StartTime;
 private String base3EndTime;
 private String base1MSRName;
 private String base2MSRName;
 private String base3MSRName;
 private String criThreshold;
 private String warThreshold;
 private String failThreshold;
 private String phase;
 private String curStartTimeAbsolute;
 private String curEndTimeAbsolute;
 private String curStartTimeElapsed;
 private String curEndTimeElapsed;
 private static String timePeriod;
 String duration;
 
 public String getTimePeriod() {
		return timePeriod;
	}

	public void setTimePeriod(String timePeriod) {
		this.timePeriod = timePeriod;
	}

	public String getCurStartTimeAbsolute() {
		return curStartTimeAbsolute;
	}

	public void setCurStartTimeAbsolute(String curStartTimeAbsolute) {
		this.curStartTimeAbsolute = curStartTimeAbsolute;
	}

	public String getCurEndTimeAbsolute() {
		return curEndTimeAbsolute;
	}

	public void setCurEndTimeAbsolute(String curEndTimeAbsolute) {
		this.curEndTimeAbsolute = curEndTimeAbsolute;
	}

	public String getCurStartTimeElapsed() {
		return curStartTimeElapsed;
	}

	public void setCurStartTimeElapsed(String curStartTimeElapsed) {
		this.curStartTimeElapsed = curStartTimeElapsed;
	}

	public String getCurEndTimeElapsed() {
		return curEndTimeElapsed;
	}

	public void setCurEndTimeElapsed(String curEndTimeElapsed) {
		this.curEndTimeElapsed = curEndTimeElapsed;
	}

	public String getPhase() {
		return phase;
	}

	public void setPhase(String phase) {
		this.phase = phase;
	}

 
 
public String getCriThreshold() {
	return criThreshold;
}

public void setCriThreshold(String criThreshold) {
	this.criThreshold = criThreshold;
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

public String getCheckProfilePath() {
	return checkProfilePath;
}

public void setCheckProfilePath(String checkProfilePath) {
	this.checkProfilePath = checkProfilePath;
}

public String getNdIntegrationUri() {
	return ndIntegrationUri;
}

public void setNdIntegrationUri(String ndIntegrationUri) {
	this.ndIntegrationUri = ndIntegrationUri;
}

public String getNdUsername() {
	return ndUsername;
}

public void setNdUsername(String ndUsername) {
	this.ndUsername = ndUsername;
}

public Secret getNdPassword() {
	return ndPassword;
}

public void setNdPassword(String ndPassword) {
	this.ndPassword = StringUtils.isEmpty(ndPassword) ? null : Secret.fromString(ndPassword);
}
 
public String getNsIntegrationUri() {
	return nsIntegrationUri;
}

public void setNsIntegrationUri(String nsIntegrationUri) {
	this.nsIntegrationUri = nsIntegrationUri;
}

public String getNsUsername() {
	return nsUsername;
}

public void setNsUsername(String nsUsername) {
	this.nsUsername = nsUsername;
}

public Secret getNsPassword() {
	return nsPassword;
}

public void setNsPassword(String nsPassword) {
	this.nsPassword = StringUtils.isEmpty(nsPassword) ? null : Secret.fromString(nsPassword);
}

//public boolean getPrevDuration()
//{
//  if(prevDuration != null)
//    return true;
//  else
//    return false;
//}
 

}
    

