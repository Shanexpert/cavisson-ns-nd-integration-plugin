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
   if(getInitDurationValues() != null)
   {
     String duration = getInitDurationValues();
     String values[] = duration.split("@");
	   
     ndParams.setInitStartTime(values[0]);
     ndParams.setInitEndTime(values[1]);
   }
     
   
  ndParams.setPrevDuration(getPrevDuration());
   
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
     NetStormReport report = dataCollector.createReportFromMeasurements();
     
     NetStormBuildAction buildAction = new NetStormBuildAction(run, report, false, true);
     
      run.addAction(buildAction);
      run.setDisplayName(NetStormBuilder.testRunNumber);
      NetStormBuilder.testRunNumber = "-1";
     
     //change status of build depending upon the status of report.
      TestReport tstRpt =  report.getTestReport();
      if(tstRpt.getOverAllStatus().equals("FAIL"))
      run.setResult(Result.FAILURE);

     logger.println("Ready building Integrated  report");
     List<NetStormReport> previousReportList = getListOfPreviousReports(run, report.getTimestamp());
     
     double averageOverTime = calculateAverageBasedOnPreviousReports(previousReportList);
     logger.println("Calculated average from previous reports for integrated: " + averageOverTime);

     double currentReportAverage = report.getAverageForMetric(DEFAULT_TEST_METRIC);
     logger.println("Metric for integrated: " + DEFAULT_TEST_METRIC + "% . Build status is: " + ((Run<?,?>) run).getResult());
   }
   catch(Exception e)
   {
     logger.println("Not able to create netstorm report for NSNDIntegrated.may be some configuration issue in running scenario.");
     return ;
   }
   
   
   return ;
               
    }
   
   public String getInitDurationValues()
   {
     if(initDuration != null)
     {
       if(initDuration.containsKey("initStartTime"))
       {
         initStartTime = (String)initDuration.get("initStartTime");
         setInitStartTime(initStartTime);
       }
       
       if(initDuration.containsKey("initEndTime"))
       {
           initEndTime = (String)initDuration.get("initEndTime");
           setInitEndTime(initEndTime);
       }   
         
     }
     
     if(initStartTime != null && initEndTime != null)
       return initStartTime+"@"+initEndTime;
     else
    	return null;
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
    
    public boolean isPrevDuration()
   {
    return getPrevDuration();
   }

    NSNDIntegrationParameterForReport  ndParams = new NSNDIntegrationParameterForReport();
    
    @DataBoundConstructor
   public NSNDIntegrationResultsPublisher(final String nsIntegrationUri, final String nsUsername, String nsPassword, final String ndIntegrationUri, final String ndUsername,
         String ndPassword, final String baseStartTime, final String baseEndTime, 
         final JSONObject prevDuration, final JSONObject initDuration, final String initEndTime,
         final String initStartTime,final String checkProfilePath, final String criThreshold,
         final String warThreshold, final String failThreshold)
  {

   
   /*creating json for sending the paramters to get the response json. */
   setNsIntegrationUri(nsIntegrationUri);
   setNsUsername(nsUsername);
   setNsPassword(nsPassword);
   setNdIntegrationUri(ndIntegrationUri);
   setNdUsername(ndUsername);
   setNdPassword(ndPassword);
   setBaseStartTime(baseStartTime);
   setBaseEndTime(baseEndTime);
 
   setCheckProfilePath(checkProfilePath);
   setCriThreshold(criThreshold);
   setWarThreshold(warThreshold);
   setFailThreshold(failThreshold);
   this.initDuration = initDuration;
   this.prevDuration = prevDuration;
   ndParams.setBaseEndTime(baseEndTime);
   ndParams.setBaseStartTime(baseStartTime);
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
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validateURLConnectionString(nsIntegrationUri);
 }
 
 public FormValidation doCheckNsPassword(@QueryParameter String nsPassword)
 {	
	   Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validatePassword(nsPassword);
 }
 
 public FormValidation doCheckNsUsername(@QueryParameter final String nsUsername)
 {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validateUsername(nsUsername);
 }
 
 public FormValidation doCheckNdIntegrationUri(@QueryParameter final String ndIntegrationUri)
 {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validateURLConnectionString(ndIntegrationUri);
 }
 
 public FormValidation doCheckNdPassword(@QueryParameter String ndPassword)
 {
	   Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validatePassword(ndPassword);
 }
 
 public FormValidation doCheckNdUsername(@QueryParameter final String ndUsername)
 {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return  FieldValidator.validateUsername(ndUsername);
 }
 
 public FormValidation doCheckWarThreshold(@QueryParameter final String warThreshold) {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return FieldValidator.validateThresholdValues(warThreshold);
} 
 
 public FormValidation doCheckCriThreshold(@QueryParameter final String criThreshold) {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return FieldValidator.validateThresholdValues(criThreshold);
} 
  
 
 public FormValidation doCheckFailThreshold(@QueryParameter final String failThreshold) {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return FieldValidator.validateThresholdValues(failThreshold);
} 
 
 public FormValidation doCheckBaseStartTime(@QueryParameter final String baseStartTime) throws ParseException {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return FieldValidator.validateDateTime(baseStartTime);
} 
 
 public FormValidation doCheckBaseEndTime(@QueryParameter final String baseEndTime) throws ParseException {
	 Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
   return FieldValidator.validateDateTime(baseEndTime);
}  
 
 /*
 Need to test connection on given credientials
 */
public FormValidation doTestNsNdIntegratedConnection(@QueryParameter("nsIntegrationUri") final String nsIntegrationUri, @QueryParameter("nsUsername") final String nsUsername, @QueryParameter("nsPassword") String nsPassword, @QueryParameter("ndIntegrationUri") final String ndIntegrationUri, @QueryParameter("ndUsername") final String ndUsername, @QueryParameter("ndPassword") String ndPassword ) 
{ 
  Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
  
  FormValidation validationResult;
  
  StringBuffer errMsg = new StringBuffer();
 
  if (FieldValidator.isEmptyField(nsIntegrationUri))
  {
    return validationResult = FormValidation.error("URL connection string for NS cannot be empty and should start with http:// or https://");
  } 
  else if (!(nsIntegrationUri.startsWith("http://") || nsIntegrationUri.startsWith("https://"))) 
  {
    return validationResult = FormValidation.error("URL connection string should start with http:// or https://");
  }
  
  if(FieldValidator.isEmptyField(nsUsername))
  {
    return validationResult = FormValidation.error("Please enter user name.");
  }

  if(FieldValidator.isEmptyField(nsPassword))
  {
    return validationResult = FormValidation.error("Please enter password.");
  }
  
  if (FieldValidator.isEmptyField(ndIntegrationUri))
  {
    return validationResult = FormValidation.error("URL connection string for ND cannot be empty and should start with http:// or https://");
  } 
  else if (!(ndIntegrationUri.startsWith("http://") || ndIntegrationUri.startsWith("https://"))) 
  {
    return validationResult = FormValidation.error("URL connection string should start with http:// or https://");
  }
  
  if(FieldValidator.isEmptyField(ndUsername))
  {
    return validationResult = FormValidation.error("Please enter user name.");
  }

  if(FieldValidator.isEmptyField(ndPassword))
  {
    return validationResult = FormValidation.error("Please enter password.");
  }
  
  NSNDIntegrationResultsPublisher.nsPassword = Secret.fromString(nsPassword);
  NSNDIntegrationResultsPublisher.ndPassword = Secret.fromString(ndPassword);
  
  NSNDIntegrationConnectionManager connection = new NSNDIntegrationConnectionManager(nsIntegrationUri, nsUsername, NSNDIntegrationResultsPublisher.nsPassword, ndIntegrationUri, ndUsername, NSNDIntegrationResultsPublisher.ndPassword, null);
  
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
 private static Secret nsPassword;
 private String ndIntegrationUri = "";
 private String ndUsername = "";
 private static Secret ndPassword;
 private JSONObject prevDuration = new JSONObject();
 private JSONObject initDuration = new JSONObject();
 private String baseStartTime;
 private String baseEndTime;
 private  String checkProfilePath;
 private String initStartTime;
 private String initEndTime;
 private String criThreshold;
 private String warThreshold;
 private String failThreshold;
 String duration;
 
 
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

public void setNsPassword(String nsPassword) {
	this.nsPassword = StringUtils.isEmpty(nsPassword) ? null : Secret.fromString(nsPassword);
}

public boolean getPrevDuration()
{
  if(prevDuration != null)
    return true;
  else
    return false;
}
 

}
    

