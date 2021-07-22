/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.AbstractProject;
import hudson.model.Action;
import hudson.model.Project;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.BuildStepMonitor;
import hudson.tasks.Builder;
import hudson.tasks.Publisher;
import hudson.tasks.Recorder;
import hudson.util.FormValidation;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONObject;
import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.omg.CORBA.portable.OutputStream;

import java.util.logging.Level;
import java.util.logging.Logger;

import jenkins.model.*;
import org.apache.commons.lang.StringUtils;
import hudson.util.Secret;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
/**
 *
 * @author preety.yadav
 */
public class NetStormResultsPublisher extends Recorder implements SimpleBuildStep  {
     private transient static final Logger logger = Logger.getLogger(NetStormResultsPublisher.class.getName());
    

    
  private static final String DEFAULT_USERNAME = "netstorm";// Default user name for NetStorm
  private static final String DEFAULT_TEST_METRIC = "Average Transaction Response Time (Secs)";// Dafault Test Metric

    public void setUsername(final String username) 
  {
    this.username = username;
  }

    public Secret getPassword() 
    {
      return password;
    }
    
  public void setPassword(final String password) 
  {
	  this.password =  StringUtils.isEmpty(password) ? null : Secret.fromString(password);
  }

  
  public String getNetstormUri() 
  {
    return netstormUri;
  }

  public void setNetstormUri(final String netstormUri) 
  {
    this.netstormUri = netstormUri;
  }

  public boolean isDurationReport() {
	return durationReport;
}

public void setDurationReport(boolean durationReport) {
	this.durationReport = durationReport;
}

public String getUsername() 
  {
    return username;
  }
  
  public String getHtmlTablePath()
  {
    if(htmlTable != null)
    {
      if(htmlTable.containsKey("htmlTablePath"))
        htmlTablePath = (String)htmlTable.get("htmlTablePath");  
    }
    return htmlTablePath;
  }
    
  
   public String getProject() {
    return project;
  }

  public void setProject(String project) {
    this.project = project;
  }
  
  public String getSubProject() {
    return subProject;
  }

  public void setSubProject(String subProject) {
    this.subProject = subProject;
  }
  
   public String getScenario() {
    return scenario;
  }

  public void setScenario(String scenario) {
    this.scenario = scenario;
  }
    
   public static class DescriptorImpl extends BuildStepDescriptor<Publisher> {

  //This is used to show post build action item
  @Override
  public String getDisplayName() 
  {
    return LocalMessages.PUBLISHER_DISPLAYNAME.toString();
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
    
 public FormValidation doCheckNetstormUri(@QueryParameter final String netstormUri)
 {
   return  FieldValidator.validateURLConnectionString(netstormUri);
 }
 
 public FormValidation doCheckPassword(@QueryParameter String password)
 {
   return  FieldValidator.validatePassword(password);
 }
 
 public FormValidation doCheckUsername(@QueryParameter String username)
 {
   return  FieldValidator.validateUsername(username);
 }
 
 public FormValidation doCheckHtmlTablePath(@QueryParameter final String htmlTablePath)
 {
   return FieldValidator.validateHtmlTablePath(htmlTablePath);
 }
     
 /*
  Need to test connection on given credientials
  */
 public FormValidation doTestNetStormConnection(@QueryParameter("netstormUri") final String netstormRestUri, @QueryParameter("username") final String username, @QueryParameter("password") String password ) 
 {

   FormValidation validationResult;
   
   StringBuffer errMsg = new StringBuffer();
  
   if (FieldValidator.isEmptyField(netstormRestUri))
   {
     return validationResult = FormValidation.error("URL connection string cannot be empty and should start with http:// or https://");
   } 
   else if (!(netstormRestUri.startsWith("http://") || netstormRestUri.startsWith("https://"))) 
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
   
   //NetStormResultsPublisher.password = Secret.fromString(password);
   
  NetStormConnectionManager connection = new NetStormConnectionManager(netstormRestUri, username, Secret.fromString(password), false);
    
   if (!connection.testNSConnection(errMsg)) 
   { 
     validationResult = FormValidation.warning("Connection to netstorm unsuccessful, due to: "+  errMsg);
   }
  else
     validationResult = FormValidation.ok("Connection successful");
 
   return validationResult;
 }
}

@Extension
public static final DescriptorImpl DESCRIPTOR = new DescriptorImpl();
 

/**
 * Below fields are configured via the <code>config.jelly</code> page.
 */
 private String netstormUri = "";
 private String username = "";
 private Secret password;
 private String project = "";
 private String subProject = "";
 private String scenario = "";
 private JSONObject htmlTable = new JSONObject();
 private String htmlTablePath = null;
 private boolean durationReport = false;
 private String profile = "";
  
 @DataBoundConstructor
 public NetStormResultsPublisher(final String netstormUri, final String username,String password, final JSONObject htmlTable,final String project, final String subProject, final String scenario, final boolean  durationReport, final String profile)
 {
   System.out.println(" getting constructor parmeter== "+netstormUri +", username = "+username+", project = "+project+", subProject = " +subProject);
     logger.log(Level.INFO, "duration check = " + durationReport + ", uri = " + netstormUri+", profile -"+profile);
   setNetstormUri(netstormUri);
   setUsername(username);
   setPassword(password);
   setProject(project);
   setSubProject(subProject);
   setScenario(scenario);
   this.durationReport = durationReport;
   setDurationReport(durationReport);
   this.htmlTable = htmlTable;
   this.profile = profile;
 }
 

 NetStormResultsPublisher(final String netstormUri, final String username,String password, final String htmlTable, final JSONObject  durationReport) {
     System.out.println(" getting constructor parmeter== "+netstormUri +", username = "+username);
    logger.log(Level.INFO, "duration  repport = " + durationReport);
   setNetstormUri(netstormUri);
   setUsername(username);
   setPassword(password);
  // setDurationCheckBox(durationReport);
 }
  
 public void setArguments()
 {
   setProject(this.project);
   setSubProject(this.subProject);
   setScenario(this.scenario); 
 }
 
 @Override
 public BuildStepDescriptor<Publisher> getDescriptor()
 {

    return DESCRIPTOR;
 }
 
 
  @Override
 public Action getProjectAction(AbstractProject<?, ?> project) 
 {
   return null;//new NetStormProjectAction(project,  NetStormDataCollector.getAvailableMetricPaths());
 }

   @Override
 public BuildStepMonitor getRequiredMonitorService() 
 {
   // No synchronization necessary between builds
   return BuildStepMonitor.NONE;
 }
 
 
 
      @Override
    public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {
     Map<String, String> env = run instanceof AbstractBuild ? ((AbstractBuild<?,?>) run).getBuildVariables() : Collections.<String, String>emptyMap();   
    PrintStream logger = taskListener.getLogger();
    StringBuffer errMsg = new StringBuffer();
    
    
     NetStormConnectionManager connection = new NetStormConnectionManager(netstormUri, username, password, durationReport);
    
    logger.println("Verify connection to NetStorm interface ..." + durationReport);
    
    if (!connection.testNSConnection(errMsg)) 
    {
      logger.println("Connection to netstorm unsuccessful, cannot to proceed to generate report.");
      logger.println("Error: " + errMsg);
      
      if (run.getResult().isBetterOrEqualTo(Result.UNSTABLE))
        run.setResult(Result.FAILURE);
      
      return ;
    }
    
    
     if (run.getResult() == Result.FAILURE || run.getResult() == Result.ABORTED) {
          return ;
        }
     
    //This is used to set html table path. 
    if(getHtmlTablePath() != null)
    {
      //connection.setHtmlTablePath(getHtmlTablePath());
      logger.println("Html Report Path set as - " + htmlTablePath);
    }
    
      if (run instanceof AbstractBuild) 
      {
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
               System.out.println("((NetStormBuilder)currentBuilder).getProject() == "+((NetStormBuilder)currentBuilder).getProject());
               connection.setProject(((NetStormBuilder)currentBuilder).getProject());
               connection.setSubProject(((NetStormBuilder)currentBuilder).getSubProject());
               connection.setScenario(((NetStormBuilder)currentBuilder).getScenario());
               connection.setProfile(((NetStormBuilder)currentBuilder).getProfile());
             }
             break;
          }
        }  
      }
      else
      {
         System.out.println("project = "+this.project+", subproject == "+this.subProject+", scenario = "+this.scenario);
         connection.setProject(this.project);
         connection.setSubProject(this.subProject);
         connection.setScenario(this.scenario);
         connection.setProfile(profile);
      }
       
    logger.println("Connection successful, continue to fetch measurements from netstorm Controller ...");
    //run.doBuildNumber(NetStormBuilder.testRunNumber);
    NetStormDataCollector dataCollector = new NetStormDataCollector(connection,  run , Integer.parseInt(NetStormBuilder.testRunNumber), "T", NetStormBuilder.testCycleNumber);
    
    try
    {
      NetStormReport report = dataCollector.createReportFromMeasurements(logger, fp);
      boolean pdfUpload = dumpPdfInWorkspace(fp, connection);
      logger.println("Pdf Uploaded = " + pdfUpload);
       boolean htmlReport = getHTMLReport(fp, connection);
     // NetStormBuildAction buildAction = new NetStormBuildAction(run, report);
      //run.addAction(buildAction);
      run.setDisplayName(NetStormBuilder.testRunNumber);
      NetStormBuilder.testRunNumber = "-1";
      logger.println("Ready building NetStorm report");
    
   // List<NetStormReport> previousReportList = getListOfPreviousReports( run, report.getTimestamp());
    
   // double currentReportAverage = report.getAverageForMetric(DEFAULT_TEST_METRIC);
   // logger.println("Current report average: " + currentReportAverage);
    
    }
    catch(Exception e)
    {
      logger.println("Not able to create netstorm report.may be some configuration issue in running scenario." + e);
      return ;
    }
    return ;
     
    }

   /*Method to dump pdf file in workspace*/
      private boolean dumpPdfInWorkspace(FilePath fp, NetStormConnectionManager connection) throws IOException, InterruptedException {
    	  /*getting testrun number*/
    	  String testRun = NetStormBuilder.testRunNumber;
    	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
    	  String dir = fp + "/TR" + testRun;
    	  
    	  
    	  logger.log(Level.INFO, "Pdf directory"+dir);
    	  
    	  
    	   FilePath fz = new FilePath(fp.getChannel(), fp + "/TR" + testRun);
    	   fz.mkdirs();
    	   FilePath fk = new FilePath(fp.getChannel(), fz + "/testsuite_report_" + testRun + ".pdf");
    	  logger.log(Level.INFO, "File path for pdf file = " + fk);
    	  
    	  try {
    		  URL urlForPdf;
    		  String str =   connection.getUrlString();
    		  urlForPdf = new URL(str+"/ProductUI/productSummary/jenkinsService/getPdfData");
    		  logger.log(Level.INFO, "urlForPdf-"+urlForPdf);

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
    			  logger.log(Level.INFO, "response 200 OK");   
    			  byte[] mybytearray = new byte[1024];
    			  InputStream is = connect.getInputStream();
    			//  FileOutputStream fos = new FileOutputStream(fp);
    			  BufferedOutputStream bos = new BufferedOutputStream(fk.write());
    			  int bytesRead;
    			  while((bytesRead = is.read(mybytearray)) > 0){
    				logger.log(Level.INFO, "bytesRead inside while check"+ bytesRead);
    				bos.write(mybytearray, 0, bytesRead);
    				//fp.write(content, null);
    			  }
    			  bos.close();
    			  is.close();
    		  } else {
    			  logger.log(Level.INFO, "ErrorCode-"+ connect.getResponseCode());
    			  logger.log(Level.INFO, "content type-" + connect.getContentType());
    		  }
    		  return true;
    	  } catch (Exception e){
    		  logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
    		  return false;
    	  }

      }  
    
    private boolean getHTMLReport(FilePath fp, NetStormConnectionManager connection) throws IOException, InterruptedException {
    	 
    	  /*getting testrun number*/
    	  String testRun = NetStormBuilder.testRunNumber;
    	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
    	  String zipFile = fp + "/TestSuiteReport.zip";
    
       	 FilePath  fz = new FilePath(fp.getChannel(), zipFile);
       	   if(fz.exists()) {
     		   fz.delete();
     		   fz = new FilePath(fp.getChannel(), zipFile);
       	   }
    	  
    	  try {
    		  URL urlForHTMLReport;
    		  String str =   connection.getUrlString();
    		  urlForHTMLReport = new URL(str+"/ProductUI/productSummary/jenkinsService/getHTMLReport");
    		  logger.log(Level.INFO, "urlForPdf-"+urlForHTMLReport);

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
    			  logger.log(Level.INFO, "response 200 OK");   
    			  byte[] mybytearray = new byte[1024];
    			  InputStream is = connect.getInputStream();
    			 // FileOutputStream fos = new FileOutputStream(file);
    			  BufferedOutputStream bos = new BufferedOutputStream(fz.write());
    			  int bytesRead;
    			  while((bytesRead = is.read(mybytearray)) > 0){
    				logger.log(Level.INFO, "bytesRead inside while check"+ bytesRead);
    				bos.write(mybytearray, 0, bytesRead);
    			  }
    			  bos.close();
    			  is.close();
    		  } else {
    			  logger.log(Level.INFO, "ErrorCode-"+ connect.getResponseCode());
    			  logger.log(Level.INFO, "content type-" + connect.getContentType());
    		  }
    		  
    		  String destDir = fp + "/TestSuiteReport";
    		  
    		  FilePath dir = new FilePath(fp.getChannel(), destDir);
    		  if(dir.exists())
    			  dir.deleteRecursive(); 
    		  dir.mkdirs();
    			 
              unzip(dir, fz);
    		  return true;
    	  } catch (Exception e){
    		  logger.log(Level.SEVERE, "Unknown exception in methid getHTMLreport. IOException -", e);
    		  e.printStackTrace();
    		  return false;
    	  }
    	  
      }
      
      private static void unzip(FilePath dir, FilePath zipFile) throws IOException, InterruptedException {
    	
    	    logger.log(Level.INFO, "inside unzip method...");
    	    try {
    	    	InputStream in = zipFile.read();
    	    	dir.unzipFrom(in);
    	    } catch (IOException e) {
    	    	logger.log(Level.SEVERE, "Exception in unzipping file = " + e);
    	        e.printStackTrace();
    	    }
    	    
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
   
   
  private List<NetStormReport> getListOfPreviousReports(final Run build, final long currentTimestamp) 
  {
    final List<NetStormReport> previousReports = new ArrayList<NetStormReport>();

   // final List<? extends AbstractBuild<?, ?>> builds = build.getProject().getBuilds();
   // final NetStormBuildAction performanceBuildAction = build.getAction(NetStormBuildAction.class);
    
    //    logger.log(Level.INFO, "data in the performence build action = "+ performanceBuildAction);
    
   // previousReports.add(performanceBuildAction.getBuildActionResultsDisplay().getNetStormReport());
    
  //  logger.log(Level.INFO, "data in the get result display = "+ performanceBuildAction.getBuildActionResultsDisplay().getNetStormReport());

    return previousReports;
  }
 
  public boolean isImportSelected()
  {
    if(getHtmlTablePath() == null)
      return false;
    else
      return true;
  }
}
