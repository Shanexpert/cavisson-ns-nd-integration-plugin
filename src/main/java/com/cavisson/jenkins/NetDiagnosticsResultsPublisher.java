package com.cavisson.jenkins;

import org.kohsuke.stapler.verb.*;
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
import java.io.InputStream;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.util.Collections;
import jenkins.tasks.SimpleBuildStep;

import net.sf.json.JSONObject;
import net.sf.json.JSONArray;
import net.sf.json.*;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;

import com.cavisson.jenkins.NdConnectionManager;
import com.cavisson.jenkins.NetDiagnosticsParamtersForReport;

import jenkins.model.*;
import com.cavisson.jenkins.FieldValidator;

import org.apache.commons.lang.StringUtils;
import hudson.util.Secret;

public class NetDiagnosticsResultsPublisher extends Recorder implements SimpleBuildStep  
{

  private static final String DEFAULT_USERNAME = "netstorm";// Default user name for NetStorm
  private static final String DEFAULT_TEST_METRIC = "Average Transaction Response Time (Secs)";// Dafault Test Metric  
  private static final String fileName = "jenkins_check_rule";
  private static PrintStream logger;

    @Override
    public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener listener) throws InterruptedException, IOException {
       Map<String, String> env = run instanceof AbstractBuild ? ((AbstractBuild<?,?>) run).getBuildVariables() : Collections.<String, String>emptyMap();    
       logger = listener.getLogger();
   StringBuffer errMsg = new StringBuffer();
   
   
  // EnvVars env = build.getEnvironment(listener);
   String curStart = "";
   String curEnd = " ";
   String path = "";
   String jobName = "";
   String criticalThreshold = "";
   String warningThreshold = "";
   String overallThreshold = "";
   Boolean fileUpload = false;

   Set keyset = env.keySet();
  System.out.println("cur start time -- "+keyset);	 
   for(Object key : keyset)
   {
     Object value = env.get(key);
     
     String keyEnv = (String)key;
     
     if(value instanceof String)
     {
       if(keyEnv.startsWith("curStartTime"))
    	 {
    	   curStart =(String) value;
    	   setCurStartTime(curStart);
    	 }
    	 
    	 if(keyEnv.startsWith("curEndTime"))
    	 {
           curEnd =(String) value;
    	   setCurEndTime(curEnd);
    	 }

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
     
   
   //ndParams.setPrevDuration(getPrevDuration());
   
   NdConnectionManager connection = new NdConnectionManager(netdiagnosticsUri, username, password, ndParams, true);
   connection.setCurStart(curStartTime);
   connection.setCurEnd(curEndTime);
   connection.setJkRule(json);
   connection.setCritical(criticalThreshold);
   connection.setWarning(warningThreshold);
   connection.setOverall(overallThreshold);  

 
   logger.println("Verify connection to NetDiagnostics interface ...");
   
   if (!connection.testNDConnection(errMsg, null, logger)) 
   {
     logger.println("Connection to netdiagnostics unsuccessful, cannot to proceed to generate report.");
     logger.println("Error: " + errMsg);
       
     return ;
   }
   logger.println("Successfully generated report on server.");
   //Need to pass test run number
   
   //For setting duration in case of netdiagnostics
   try{
	SimpleDateFormat trDateFormat = new SimpleDateFormat("MM/dd/yy HH:mm:ss");
	Date userDate = trDateFormat.parse(curStartTime); 
	long absoluteDateInMillies = userDate.getTime();	 
	   
	Date userEndDate = trDateFormat.parse(curEndTime); 
	long absoluteEndDateInMillies = userEndDate.getTime();	 
	   
	   
	long durationCal = absoluteEndDateInMillies - absoluteDateInMillies;
        long seconds = durationCal / 1000;
        long s = seconds % 60;
        long m = (seconds / 60) % 60;
        long h = (seconds / (60 * 60)) % 24;
        duration =  String.format("%d:%02d:%02d", h,m,s);
	//duration = (int) (durationCal / (1000*60));
        
        
	    
      } catch(Exception ex){
    }

   NetStormDataCollector dataCollector = new NetStormDataCollector(connection, run , 50000, "T", true, duration);
   
   
   try
   {
    // NetStormReport report = dataCollector.createReportFromMeasurements(logger);
	   NetStormReport report  = null;
	   
	   
	   boolean pdfUpload = dumpPdfInWorkspace(fp, connection);
	   boolean htmlReport = getHTMLReport(fp, connection);
     logger.println("Pdf Uploaded = " + pdfUpload);
     
  //   NetStormBuildAction buildAction = new NetStormBuildAction(run, report, true);
     
   // run.addAction(buildAction);
   
     //change status of build depending upon the status of report.
//     TestReport tstRpt =  report.getTestReport();
//      if(tstRpt.getOverAllStatus().equals("FAIL"))
//        run.setResult(Result.FAILURE);
//
//     logger.println("Ready building NetDiagnostics report");
//     List<NetStormReport> previousReportList = getListOfPreviousReports(run, report.getTimestamp());
//     
//     double averageOverTime = calculateAverageBasedOnPreviousReports(previousReportList);
//     logger.println("Calculated average from previous reports: " + averageOverTime);
//
//     double currentReportAverage = report.getAverageForMetric(DEFAULT_TEST_METRIC);
//     logger.println("Metric: " + DEFAULT_TEST_METRIC + "% . Build status is: " + ((Run<?,?>) run).getResult());
   }
   catch(Exception e)
   {
     logger.println("Not able to create netstorm report.may be some configuration issue in running scenario.");
     return ;
   }
   
   
   return ;
               
 }
    
    private boolean getHTMLReport(FilePath fp, NdConnectionManager connection) {
   	 
    	String destDirectory = fp + "/TestSuiteReport";
		  
		  File dirr = new File(destDirectory);
		  if(dirr.exists())
			  dirr.delete();
  	  /*getting testrun number*/
  	  String testRun = NetDiagnosticsResultsPublisher.testRun;
  	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
  	  String zipFile = fp + "/TestSuiteReport.zip";
  	 // logger.log(Level.INFO, "Pdf directory"+zipFile);
  	 
  	  File file = new File(zipFile);
//  	  if(file.exists()) {
//  		  file.delete();
//  	  }
  	  
  	  try {
  		  URL urlForHTMLReport;
  		  String str =   connection.getUrlString();
  		  urlForHTMLReport = new URL(str+"/ProductUI/productSummary/jenkinsService/getHTMLReport");
  		//  logger.log(Level.INFO, "urlForPdf-"+urlForHTMLReport);

  		  HttpURLConnection connect = (HttpURLConnection) urlForHTMLReport.openConnection();
  		  connect.setConnectTimeout(0);
  		  connect.setReadTimeout(0);
  		  connect.setRequestMethod("POST");
  		  connect.setRequestProperty("Content-Type", "application/octet-stream");

  		  connect.setDoOutput(true);
  		  java.io.OutputStream outStream = connect.getOutputStream();
  		  outStream.write(testRun.getBytes());
  		  outStream.flush();

  		  if (connect.getResponseCode() == 200) {
  			 // logger.log(Level.INFO, "response 200 OK");   
  			  byte[] mybytearray = new byte[1024];
  			  InputStream is = connect.getInputStream();
  			  FileOutputStream fos = new FileOutputStream(file);
  			  BufferedOutputStream bos = new BufferedOutputStream(fos);
  			  int bytesRead;
  			  while((bytesRead = is.read(mybytearray)) > 0){
  			//	logger.log(Level.INFO, "bytesRead inside while check"+ bytesRead);
  				bos.write(mybytearray, 0, bytesRead);
  			  }
  			  bos.close();
  			  is.close();
  		  } else {
  			  //logger.log(Level.INFO, "ErrorCode-"+ connect.getResponseCode());
  			  //logger.log(Level.INFO, "content type-" + connect.getContentType());
  		  }
  		  
  		  String destDir = fp + "/TestSuiteReport";
  		  
  		  File dir = new File(destDir);
		  if(dir.exists())
			  dir.delete();
          unzip(zipFile, destDir);
  		  return true;
  	  } catch (Exception e){
  		  e.printStackTrace();
  		  return false;
  	  }
  	  
    }
    
    private static void unzip(String zipFilePath, String destDir) {
	    File dir = new File(destDir);
	    // create output directory if it doesn't exist
	    if(!dir.exists()) dir.mkdirs();
	    FileInputStream fis;
	    //buffer for read and write data to file
	    byte[] buffer = new byte[1024];
	    try {
	        fis = new FileInputStream(zipFilePath);
	        ZipInputStream zis = new ZipInputStream(fis);
	        ZipEntry ze = zis.getNextEntry();
	        while(ze != null){
	            String fileName = ze.getName();
	            File newFile = new File(destDir + File.separator + fileName);
	            System.out.println("Unzipping to "+newFile.getAbsolutePath());
	            //create directories for sub directories in zip
	            new File(newFile.getParent()).mkdirs();
	            FileOutputStream fos = new FileOutputStream(newFile);
	            int len;
	            while ((len = zis.read(buffer)) > 0) {
	            fos.write(buffer, 0, len);
	            }
	            fos.close();
	            //close this ZipEntry
	            zis.closeEntry();
	            ze = zis.getNextEntry();
	        }
	        //close last ZipEntry
	        zis.closeEntry();
	        zis.close();
	        fis.close();
	    } catch (IOException e) {
	        e.printStackTrace();
	    }
	    
	}
    
    /*Method to dump pdf file in workspace*/
    private boolean dumpPdfInWorkspace(FilePath fp, NdConnectionManager connection) {
  	  /*getting testrun number*/
  	  String testRun = NetDiagnosticsResultsPublisher.testRun;
  	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
  	  String dir = fp + "/TR" + testRun;
  	  File fl = new File(dir);
  	  if(fl.exists()) 
  		  fl.delete();
  	  
  		  fl.mkdir();
  	  

  	  File file = new File(dir + "/testsuite_report_" + testRun + ".pdf");
  	  try {
  		  URL urlForPdf;
  		  String str =   connection.getUrlString();
  		  urlForPdf = new URL(str+"/ProductUI/productSummary/jenkinsService/getPdfData");
  		  
  		  HttpURLConnection connect = (HttpURLConnection) urlForPdf.openConnection();
  		  connect.setConnectTimeout(0);
  		  connect.setReadTimeout(0);
  		  connect.setRequestMethod("POST");
  		  connect.setRequestProperty("Content-Type", "application/octet-stream");

  		  connect.setDoOutput(true);
  		  java.io.OutputStream outStream = connect.getOutputStream();
  		  outStream.write(testRun.getBytes());
  		  outStream.flush();

  		  if (connect.getResponseCode() == 200) {
  			  byte[] mybytearray = new byte[1024];
  			  InputStream is = connect.getInputStream();
  			  FileOutputStream fos = new FileOutputStream(file);
  			  BufferedOutputStream bos = new BufferedOutputStream(fos);
  			  int bytesRead;
  			  while((bytesRead = is.read(mybytearray)) > 0){
  				bos.write(mybytearray, 0, bytesRead);
  			  }
  			  bos.close();
  			  is.close();
  		  } else {
  		  }
  		  return true;
  	  } catch (Exception e){
  		  return false;
  	  }

    }  
    
    
    public void geterate(AbstractBuild<?,?> job) {
        
       
    }
  
  public static class DescriptorImpl extends BuildStepDescriptor<Publisher> {

  //This is used to show post build action item
  @Override
  public String getDisplayName()
  {
    return LocalMessages.ND_PUBLISHER_DISPLAYNAME.toString();
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
 public FormValidation doCheckNetdiagnosticsUri(@QueryParameter final String netdiagnosticsUri)
 {
   return  FieldValidator.validateURLConnectionString(netdiagnosticsUri);
 }
 
 public FormValidation doCheckPassword(@QueryParameter String password)
 {
   return  FieldValidator.validatePassword(password);
 }
 
 public FormValidation doCheckUsername(@QueryParameter final String username)
 {
   return  FieldValidator.validateUsername(username);
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
@POST
public FormValidation doTestNetDiagnosticsConnection(@QueryParameter("netdiagnosticsUri") final String netdiagnosticRestUri, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("curStartTime") final String curStartTime,@QueryParameter("curEndTime") final String curEndTime,@QueryParameter("baseStartTime") final String baseStartTime,@QueryParameter("baseEndTime") final String baseEndTime,@QueryParameter("criThreshold") final String criThreshold,@QueryParameter("warThreshold") final String warThreshold,@QueryParameter("failThreshold") final String failThreshold,@QueryParameter("initDuration") final Boolean initDuration,@QueryParameter("initStartTime") final String initStartTime,@QueryParameter("initEndTime") final String initEndTime) 
{
  Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER); 
  FormValidation validationResult;
  
  StringBuffer errMsg = new StringBuffer();
 
  if (FieldValidator.isEmptyField(netdiagnosticRestUri))
  {
    return validationResult = FormValidation.error("URL connection string cannot be empty and should start with http:// or https://");
  } 
  else if (!(netdiagnosticRestUri.startsWith("http://") || netdiagnosticRestUri.startsWith("https://"))) 
  {
    return validationResult = FormValidation.error("URL connection st should start with http:// or https://");
  }
  
  if(FieldValidator.isEmptyField(username))
  {
    return validationResult = FormValidation.error("Please enter user name.");
  }

  if(FieldValidator.isEmptyField(password))
  {
    return validationResult = FormValidation.error("Please enter password.");
  }
  
  /*validations*/
  if(FieldValidator.isEmptyField(criThreshold)) {
		return validationResult = FormValidation.error("Critical threshold can not be empty.");
  }
  if(FieldValidator.isEmptyField(warThreshold)) {
	return validationResult = FormValidation.error("Warning threshold can not be empty.");
  }
  if(FieldValidator.isEmptyField(failThreshold)) {
		return validationResult = FormValidation.error("Overall Fail Criteria can not be empty.");
  }

  if(FieldValidator.isEmptyField(curStartTime) || FieldValidator.isEmptyField(curEndTime)) {
	  return validationResult = FormValidation.error("Current Time Period can not be empty.");
  }
  if(!FieldValidator.isEmptyField(baseStartTime) || !FieldValidator.isEmptyField(baseEndTime)) {
	   if(FieldValidator.isEmptyField(baseStartTime)) {
		   return validationResult = FormValidation.error("Baseline Time Period can not be empty.");
	   }else if(FieldValidator.isEmptyField(baseEndTime)) {
		   return validationResult = FormValidation.error("Baseline Time Period can not be empty.");
	   }
  }
  if(initDuration == true) {
	  if(FieldValidator.isEmptyField(initStartTime) || FieldValidator.isEmptyField(initEndTime)) {
		  return validationResult = FormValidation.error("Initial Time Period can not be empty.");
	  }
  }
  
  
  NdConnectionManager connection = new NdConnectionManager(netdiagnosticRestUri, username, Secret.fromString(password), true);
  
  String check = netdiagnosticRestUri + "@@" + username +"@@" + password;
  if (!connection.testNDConnection(errMsg, check, logger)) 
  { 
    validationResult = FormValidation.warning("Connection to netdiagnostics unsuccessful, due to: "+  errMsg);
  }
  else
    validationResult = FormValidation.ok("Connection successful");

  return validationResult;
}
@POST
public synchronized ListBoxModel doFillProfileItems(@QueryParameter("netdiagnosticsUri") final String netdiagnosticRestUri, @QueryParameter("username") final String username, @QueryParameter("password") String password)
{
  Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
	
  ListBoxModel models = new ListBoxModel();
  StringBuffer errMsg = new StringBuffer();
  
  //IF creadentials are null or blank
  if(netdiagnosticRestUri == null || netdiagnosticRestUri.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
  {
    models.add("---Select Profile ---");   
    return models;
  }  
  
  //Making connection server to get project list
  NdConnectionManager connection = new NdConnectionManager(netdiagnosticRestUri, username, Secret.fromString(password), true);
 
  ArrayList<String> profileList = connection.getProfileList(errMsg);
  
  //IF project list is found null
  if(profileList == null || profileList.size() == 0)
  {
    models.add("---Select Profile ---");   
    return models;
  }
  
  for(String profile : profileList)
    models.add(profile);
  
  return models;
}

}
 @Extension
public static final DescriptorImpl DESCRIPTOR = new DescriptorImpl();

 

@Override
 public BuildStepDescriptor<Publisher> getDescriptor()
 {
    return DESCRIPTOR;
 }
 
 private String netdiagnosticsUri = "";
 private String username = "";
 private Secret password;
 //private JSONObject prevDuration = new JSONObject();
 //private JSONObject initDuration = new JSONObject();
 private  String curStartTime;
 private String curEndTime;
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
 String duration;
 private String profile;
 public static String testRun = "-1";
 
 
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

public String getCheckProfilePath() {
	return checkProfilePath;
}

public void setCheckProfilePath(String checkProfilePath) {
	this.checkProfilePath = checkProfilePath;
}

public String getNetdiagnosticsUri() {
	return netdiagnosticsUri;
}

public void setNetdiagnosticsUri(String netdiagnosticsUri) {
	this.netdiagnosticsUri = netdiagnosticsUri;
}

public String getUsername() {
	return username;
}

public void setUsername(String username) {
	this.username = username;
}

public Secret getPassword() {
	return password;
}

public void setPassword(String password) {
	this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
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

public String getProfile() {
	return profile;
}

public void setProfile(String profile) {
	this.profile = profile;
}

//public boolean getPrevDuration()
//{
//  if(this.prevDuration != null)
//    return true;
//  else
//    return false;
//}
NetDiagnosticsParamtersForReport  ndParams = new NetDiagnosticsParamtersForReport();

@DataBoundConstructor
 public NetDiagnosticsResultsPublisher(final String netdiagnosticsUri, final String username,
         final String password, final String base1StartTime, final String base1EndTime, final String base2EndTime,
         final String base2StartTime, final String base3StartTime, final String base3EndTime, final String checkProfilePath, final String criThreshold,
         final String warThreshold, final String failThreshold, final String curStartTime, final String curEndTime, 
         final String base1MSRName, final String base2MSRName, final String base3MSRName, final String profile)
 {
   /*creating json for sending the paramters to get the response json. */
   setNetdiagnosticsUri(netdiagnosticsUri);
   setUsername(username);
    
   setPassword(password);
   setBase1StartTime(base1StartTime);
   setBase1EndTime(base1EndTime);
   setBase1MSRName(base1MSRName);
   setBase2StartTime(base2StartTime);
   setBase2EndTime(base2EndTime);
   setBase2MSRName(base2MSRName);
   setBase3StartTime(base3StartTime);
   setBase3EndTime(base3EndTime);
   setBase3MSRName(base3MSRName);
 
   if(curStartTime != null)
    setCurStartTime(curStartTime);
   else
    setCurStartTime(this.getCurStartTime());

   if(curEndTime != null)
     setCurEndTime(curEndTime);
   else
     setCurEndTime(this.getCurEndTime());
   setCheckProfilePath(checkProfilePath);
   setCriThreshold(criThreshold);
   setWarThreshold(warThreshold);
   setFailThreshold(failThreshold);
   setProfile(profile);
   //this.initDuration = initDuration;
   //this.prevDuration = prevDuration;
   ndParams.setCheckProfilePath(checkProfilePath);
   ndParams.setBase1MSRName(base1MSRName);
   ndParams.setBase1StartTime(base1StartTime);
   ndParams.setBase1EndTime(base1EndTime);
   ndParams.setBase2EndTime(base2EndTime);
   ndParams.setBase2StartTime(base2StartTime);
   ndParams.setBase2MSRName(base2MSRName);
   ndParams.setBase3EndTime(base3EndTime);
   ndParams.setBase3StartTime(base3StartTime);
   ndParams.setBase3MSRName(base3MSRName);
   
   ndParams.setCheckProfilePath(checkProfilePath);
   ndParams.setProfile(profile);
   ndParams.setUsername(username);


  //Handling for pipeline jobs
//   if(initialDuration && initDuration == null){
//      
//      this.initDuration = new JSONObject();
//      this.initDuration.put("initStartTime", initStartTime);
//      this.initDuration.put("initEndTime", initEndTime);
//    }
//   
//    if(previousDuration && prevDuration == null){
//      this.prevDuration = new JSONObject();
//      this.prevDuration.put("prevDuration",true);
//    }


   if(this.getCurEndTime() != "")
      ndParams.setCurEndTime(this.getCurEndTime());
    else
      ndParams.setCurEndTime(curEndTime);
    
    if(this.getCurStartTime() != "")
      ndParams.setCurStartTime(this.getCurStartTime());
    else
      ndParams.setCurStartTime(curStartTime);
    
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

 public BuildStepMonitor getRequiredMonitorService()
 {
   // No synchronization necessary between builds
   return BuildStepMonitor.NONE;
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

//     final List<? extends Run<?, ?>> builds = build.getProject().getBuilds();

    
//     for (Run<?, ?> currentBuild : builds) 
//     {
//       final NetStormBuildAction performanceBuildAction = currentBuild.getAction(NetStormBuildAction.class);
//       if (performanceBuildAction == null) 
//       {
//         continue;
//       }
//       
//       final NetStormReport report = performanceBuildAction.getBuildActionResultsDisplay().getNetStormReport();
//       
//       if (report != null && (report.getTimestamp() != currentTimestamp || builds.size() == 1)) 
//       {
//         previousReports.add(report);
//       }
//     }

     return previousReports;
   }

//   public boolean isInitDuration()
//   {
//     if(getInitDurationValues() == null)
//       return false;
//     else
//       return true;
//   }
   
//   public boolean isPrevDuration()
//   {
//    return getPrevDuration();
//   }
//   
   
//   public String getInitDurationValues()
//   {
//     if(this.initDuration != null)
//     {
//       if(this.initDuration.containsKey("initStartTime"))
//       {
//         initStartTime = (String)this.initDuration.get("initStartTime");
//         setInitStartTime(initStartTime);
//       }
//       
//       if(this.initDuration.containsKey("initEndTime"))
//       {
//           initEndTime = (String)this.initDuration.get("initEndTime");
//           setInitEndTime(initEndTime);
//       }   
//         
//     }
//     
//     if(initStartTime != null && initEndTime != null)
//       return initStartTime+"@"+initEndTime;
//     else
//    	return null;
 //  }
}
