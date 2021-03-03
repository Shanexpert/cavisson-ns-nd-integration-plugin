package com.cavisson.jenkins;

import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.AbstractProject;
import hudson.model.BuildListener;
import hudson.model.FreeStyleProject;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.model.User;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.Builder;
import hudson.util.FormValidation;
import hudson.util.ListBoxModel;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONObject;
import net.sf.json.*;

import hudson.Extension;
import hudson.util.FormValidation;
import hudson.util.Secret;
import jenkins.model.Jenkins;
import org.apache.commons.lang.StringUtils;


import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.bind.JavaScriptMethod;

/**
 */
public class NetStormBuilder extends Builder implements SimpleBuildStep {

    private final String project;
    private final String subProject;
    private final String scenario;
    private final String URLConnectionString;
    private final String username;
    private final Secret password;
    private final String testMode ;
    private final String defaultTestMode = "true";
    private transient static final Logger logger = Logger.getLogger(NetStormBuilder.class.getName());
    private final String baselineType;
    private final String pollInterval;
    public static String testRunNumber = "-1";
    public static String testCycleNumber = "";
    private static final String fileName = "jenkins_check_rule_for_NS.txt";
    private String protocol="";
    private String repoIp="";
    private String repoPort="";
    private String repoPath="";
    private String repoUsername="";
    private String repoPassword="";
    private String gitPull = "";


    @DataBoundConstructor
    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String gitPull) {
    	 logger.log(Level.INFO, "inside a constructor..............gitpull -"+gitPull);
        
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.protocol = protocol;
        this.repoIp = repoIp;
        this.repoPort = repoPort;
        this.repoPath = repoPath;
        this.repoUsername = repoUsername;
        this.repoPassword = repoPassword;
        this.gitPull = gitPull;
    }

    public String getProject() 
    {
      return project;
    }
    
    public String getDefaultTestMode()
    {
      return defaultTestMode;
    }

    public String getSubProject() {
        return subProject;
    }

    public String getScenario() {
        return scenario;
    }

    public String getURLConnectionString() {
        return URLConnectionString;
    }

    public String getUsername() {
        return username;
    }
    
    public Secret getPassword() {
         return password;
    }
   
    public String getTestMode() 
    {
      return testMode;
    }
    
   
    
    public String getBaselineType() {
    
      return baselineType;
    }
    
    public String getPollInterval() {
    	return pollInterval;
    }
    
      
  public String getProtocol() {
		return protocol;
	}

	public String getRepoIp() {
		return repoIp;
	}

	public String getRepoPort() {
		return repoPort;
	}

	public String getRepoPath() {
		return repoPath;
	}

	public String getRepoUsername() {
		return repoUsername;
	}

	public String getRepoPassword() {
		return repoPassword;
	}

	public String getGitPull() {
		return gitPull;
	}

	public void setGitPull(String gitPull) {
		this.gitPull = gitPull;
	}

@Override
  public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {
   
   Boolean fileUpload = false;

   Map<String, String> envVarMap = run instanceof AbstractBuild ? ((AbstractBuild<?, ?>) run).getBuildVariables() : Collections.<String, String>emptyMap();
   PrintStream logg = taskListener.getLogger();
   
   logg.println("Calling NetstormConnectionManager constructor ..........");
   
   NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, project, subProject, scenario, testMode, baselineType, pollInterval,gitPull);      
   StringBuffer errMsg = new StringBuffer();
   
   logg.println("Starting loop ............");
   
    @SuppressWarnings("rawtypes")
      Set keyset = envVarMap.keySet();
    
     String path = "";
     String jobName = "";
     String automateScripts = "";
      for(Object key : keyset)
      {
        Object value = envVarMap.get(key);
       	
       	if(key.equals("JENKINS_HOME")) {
 	     path = (String)envVarMap.get(key);
       	}
       			
       			
        if(value instanceof String)
        {
           String envValue = (String) value;
           
           if(envValue.startsWith("NS_SESSION"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 2)
             {
                netstormConnectionManger.setDuration(temp[2]);
             }
           }
           else if(envValue.startsWith("NS_NUM_USERS"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 3)
                netstormConnectionManger.setvUsers(temp[3]);
           }  
           else if(envValue.startsWith("NS_SERVER_HOST"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 3)
                netstormConnectionManger.setServerHost(temp[3]);
           }  
           else if(envValue.startsWith("NS_SLA_CHANGE"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 3)
                netstormConnectionManger.addSLAValue(key.toString() , temp [3] );
           }
           else if(envValue.startsWith("NS_RAMP_UP_SEC") || envValue.startsWith("NS_RAMP_UP_MIN") || envValue.startsWith("NS_RAMP_UP_HR"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 4)
                netstormConnectionManger.setRampUp(temp[4] + "_" + temp[3]);
           }
           else if(envValue.startsWith("NS_TNAME"))
           {
             String tName = getSubString(envValue, 2, "_");
             if(!tName.equals(""))
               netstormConnectionManger.settName(tName);
           }
           else if(envValue.startsWith("NS_AUTOSCRIPT"))
           {
        	   logg.println("Inside Autoscript check .........."+envValue);
             String temp [] = envValue.split("_", 3);
             logg.println("temp length .........."+temp.length);
             logg.println("auto sccript string ....."+automateScripts);
             if(temp.length > 2){
//                netstormConnectionManger.setAutoScript(temp[2]);
            	 if(automateScripts.equals(""))
            		 automateScripts = temp[2];
            	 else
            		 automateScripts = automateScripts + "," +temp[2];
             }
           }
	   else if(envValue.startsWith("NS_RAMP_UP_DURATION")){
        	   String temp [] = envValue.split("_");
               if(temp.length > 4)
                  netstormConnectionManger.setRampUpDuration(temp[4]);
               logg.println("Ramp up duration .........."+temp[4]);
               logg.println("getRampUpDuration ="+netstormConnectionManger.getRampUpDuration());
           }
           
           if(envValue.equalsIgnoreCase(fileName))
           {
             fileUpload = true;
           }
        }
      }
      
      netstormConnectionManger.setAutoScript(automateScripts);
      logg.println("set auto script ="+netstormConnectionManger.getAutoScript());
     
      if(testMode == null)
      {
    	  logg.println("Please verify configured buit step, test profile mode is not selected.");
        run.setResult(Result.FAILURE);
        //return false;
      }
      
      
      if(scenario.equals("") || scenario == null || scenario.equals("---Select Scenarios ---")){
    	  if(getTestMode().equals("N"))
    		  logg.println("Please verify configured build step, scenario is not selected.");
    	  else
    		  logg.println("Please verify configured build step, Test Suite is not selected.");
    	  run.setResult(Result.FAILURE);
    	  return;
      }
      
      if(getTestMode().equals("N"))
    	  logg.println("Starting test with scenario(" + project + "/" + subProject + "/" + scenario + ")");
      else
    	  logg.println("Starting test with test suite(" + project + "/" + subProject + "/" + scenario + ")");
      

      logg.println("NetStorm URI: " + URLConnectionString );
      
      JSONObject json = null;
      
      if(fileUpload)
       {
    	json = createJsonForFileUpload(fp, logg);
    	 
       }
        
      netstormConnectionManger.setJkRule(json);
      HashMap result = netstormConnectionManger.startNetstormTest(errMsg ,logg);
      
      boolean status = (Boolean )result.get("STATUS");
      
      logg.println("Test Run Status - " + status);
      
      if(result.get("TESTRUN") != null && !result.get("TESTRUN").toString().trim().equals(""))
      {
        try
        {
        	logg.println("Test Run  - " + result.get("TESTRUN"));
          //run.set
          run.setDisplayName((String)result.get("TESTRUN"));
          
          /*set a test run number in static refrence*/
          testRunNumber = (String)result.get("TESTRUN");
          testCycleNumber = (String) result.get("TEST_CYCLE_NUMBER");         
 
          if(result.get("ENV_NAME") != null && !result.get("ENV_NAME").toString().trim().equals(""))
            run.setDescription((String)result.get("ENV_NAME")); 
         
          
          //To set the host and user name in a file for using in other publisher.
          logg.println("path  - " + path);
          File dir = new File(path.trim()+"/Property");
          if (!dir.exists()) {
              if (dir.mkdir()) {
                  System.out.println("Directory is created!");
              } else {
                  System.out.println("Failed to create directory!");
              }
          }
          
          File file = new File(path.trim()+"/Property/" +((String)result.get("TESTRUN")).trim()+"_CavEnv.property");
          
          if(file.exists())
           file.delete();
          else
          {
            file.createNewFile();
            
            try
            {
              FileWriter fw = new FileWriter(file, true);
              BufferedWriter bw = new BufferedWriter(fw);
              bw.write("HostName="+URLConnectionString);
              bw.write("\n");
              bw.write("UserName="+username);
              bw.close();
            }
            catch (Exception e){
            	 System.out.println("Exception in writing in file - "+e);
            }
         }
          
          run.setResult(Result.SUCCESS);
        }
        catch(Exception e)
        {
        	System.out.println("Unknown exception. IOException -"+e);
        }
      }
      else
       run.setResult(Result.FAILURE);
      
      //return status;
   
   
  }  
  
public void getGitConfigurationFromNS(){
	  try{
		  System.out.println("In method getGitConfigurationFromNS ...");
		  logger.log(Level.INFO, "In method getGitConfigurationFromNS ...");
		  
		  NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, false);
		  String res = netstormConnectionManger.getGitConfiguration();
		  logger.log(Level.INFO, "getGitConfigurationFromNS res ..."+res);
		  System.out.println("getGitConfigurationFromNS resss ..."+res);
		  if(res == null||res.equals("")||res.equals("notConfigured")){
			  logger.log(Level.INFO, "git is not configured ...");
			  repoIp = "";
			  repoPort = "";
			  repoPath = "";
			  repoUsername = "";
			  repoPassword = "";
			  protocol = "";
		  }else{
			  String[] resArr = res.split(" ");
//			  gitlab.com 443 vikasverma4795/demo vikasverma4795 Cavisson1! true NA NA https false
			  if(resArr.length>8){
			  repoIp = resArr[0];
			  repoPort = resArr[1];
			  repoPath = resArr[2];
			  repoUsername = resArr[3];
			  repoPassword = resArr[4];
			  protocol = resArr[8];
			  }
		  }
	  }catch(Exception e){
		  logger.log(Level.SEVERE, "Unknown exception in getGitConfigurationFromNS.", e);
	  }
}

  /*Method is used to create json for check rule*/
  public JSONObject createJsonForFileUpload(FilePath fp, PrintStream logger) {
	  try {
		  JSONObject json = null;
		  String fileNm = fileName;
		  if(fileName.indexOf(".") != -1) {
			  String name[] = fileName.split("\\.");
			  fileNm = name[0];
		  }
		  File file = new File(fp +"/"+fileNm);
		  logger.println("File path" + file);
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
		  return json;
	  } catch(Exception e) {
		  System.out.println("Unknown exception. IOException -"+ e);
		  return null;
	  }
  }
    
     /*
      *  Method which is used to start a test 
      * it makes a connection with the m/c and authenticate
      *
     */
  public String  startTest(NetStormConnectionManager netstormConnectionManger) {
      try {
          StringBuffer errBuf = new  StringBuffer();
          
           File tempFile = File.createTempFile("myfile", ".tmp");
           FileOutputStream fout = new FileOutputStream(tempFile);
          PrintStream pout=new PrintStream(fout); 
          
          //NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password,
          //project, subProject, scenario, testMode, baselineType, pollInterval);
          
          HashMap result =   netstormConnectionManger.startNetstormTest(errBuf , pout);
          
      
        if(result.get("TESTRUN") != null && !result.get("TESTRUN").toString().trim().equals(""))
        {
          /*set a test run number in static refrence*/
          testRunNumber = (String)result.get("TESTRUN");
          testCycleNumber = (String) result.get("TEST_CYCLE_NUMBER");     
          return result.toString();
        }
      
        return result.toString();
      }catch(Exception e) {
          System.out.println("Error in startin a test"+ e);
          return "Error in starting a test";
      }
  } 
    
   
    /**      
	 * @param OrgString
	 * @param startIndex
	 * @param seperator
	 * @return
	 * ex.--  OrgString = NS_TNAME_FIRST_TEST ,startIndex = 2 ,seperator = "_" .
	 * 
	 *      ("NS_TNAME_FIRST_TEST", 2 , "_")   method returns FIRST_TEST.
	 *      
	 */
    public String getSubString(String OrgString, int startIndex, String seperator)
    {
      String f[] = OrgString.split(seperator);
      String result = "";
      if(startIndex <= f.length-1)
      {
        for(int i = startIndex ; i < f.length; i++)
	{
          if(i == startIndex)
	    result  = result + f[i] ;
          else
            result  = result + "_" + f[i]  ;
	}
      }
      return result;
    }
  
    @Override
    public Descriptor getDescriptor() {
        return (Descriptor) super.getDescriptor();
    }

 

    @Extension
    public static class Descriptor extends BuildStepDescriptor<Builder> 
    {
        public Descriptor() 
        {
            load();
        }

        @Override
        public boolean configure(StaplerRequest req, JSONObject json) throws FormException {
            
        		  save();
            return true;
        }

        @Override
        public boolean isApplicable(Class<? extends AbstractProject> jobType) {
            return FreeStyleProject.class.isAssignableFrom(jobType);
        }

        @Override
        public Builder newInstance(StaplerRequest req, JSONObject formData) throws FormException
        {
          return super.newInstance(req, formData);    //To change body of overridden methods use File | Settings | File Templates.
        }

        @Override
        public String getDisplayName() {
            return Messages.NetStormBuilder_Task();
        }
        
        /**
         * 
         * @param password
         * @return 
         */
        public FormValidation doCheckPassword(@QueryParameter String password) {
        	
            return FieldValidator.validatePassword(password);
        }
        
        /**
         * 
         * @param username
         * @return 
         */
        public FormValidation doCheckUsername(@QueryParameter final String username) {
            return FieldValidator.validateUsername(username);
        }
        
        /**
         * 
         * @param URLConnectionString
         * @return 
         */
        public FormValidation doCheckURLConnectionString(@QueryParameter final String URLConnectionString)
        {
            return FieldValidator.validateURLConnectionString(URLConnectionString);
        }
        
        @JavaScriptMethod
        public ArrayList<String> getPulledObjects(String value,String URLConnectionString,String username,String password,String project,String subProject,String testMode){
        	try{
        		logger.log(Level.INFO,"getPulledObjects args - value :"+value+",url -"+URLConnectionString+", project -"+project+", subProject -"+subProject+",testMode -"+testMode);
        		ArrayList<String> res = new ArrayList<String>();
        		
        		StringBuffer errMsg = new StringBuffer();
        		if(!URLConnectionString.equals("")&&!URLConnectionString.equals("NA")&&!URLConnectionString.equals(" ")&&URLConnectionString != null&&!password.equals("") && !password.equals("NA")&&!password.equals(" ") && password != null&&!username.equals("") && !username.equals("NA") && !username.equals(" ") && username != null){
        			NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
        			if(value.equals("P")){
        				logger.log(Level.INFO,"in P.......");
        				res = connection.getProjectList(errMsg);
        			}else if(value.equals("SP")){
        				logger.log(Level.INFO,"in SP.......");
        				res = connection.getSubProjectList(errMsg, project);
        			}else if(value.equals("S")){
        				logger.log(Level.INFO,"in S.......");
        				res = connection.getScenarioList(errMsg, project, subProject, testMode);
        			}
            	}
        		if(res!=null && res.size()>0){
        			for(int i=0;i<res.size();i++){
        				logger.log(Level.INFO, "res items -"+res.get(i));
        			}
        		}
        		
        		return res;
        	}catch(Exception e){
      		logger.log(Level.SEVERE,"Exception in getPulledObjects -"+e);
      		return null;
      	}
      }
        
        /**
         * 
         * @param project
         * @return 
         */
        //public FormValidation doCheckProject(@QueryParameter final String project)
        //{
        //  return FieldValidator.validateProject(project);
        //}
        
        /**
         * 
         * @param subProject
         * @return 
         */ 
        //public FormValidation doCheckSubProject(@QueryParameter final String subProject)
        //{
        //  return FieldValidator.validateSubProjectName(subProject);
        //}
        
        /**
         * 
         * @param scenario
         * @return 
         */
        //public FormValidation doCheckScenario(@QueryParameter final String scenario)
        //{
        //    return FieldValidator.validateScenario(scenario);
        //}
        
        /**
         * 
         * @param gitPull
         * @return 
         */
//        public FormValidation doCheckGitPull(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter String gitPull,@QueryParameter String project,@QueryParameter final String subProject,@QueryParameter final String testMode)
//        {
//        	logger.log(Level.INFO, "gitPull in doCheckGitPull -"+gitPull);
//        	FormValidation validationResult;
//        	
//        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
//        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password");
//        		return validationResult;
//        	}
//        	
//        	if(gitPull.equals("true")){
//            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
//            JSONObject res = netstormConnectionManger.pullObjectsFromGit();
//            logger.log(Level.INFO, "project before ..."+project);
//            if(res != null && !res.isEmpty()){
//            	if(!res.get("ErrMsg").toString().equals("")){
//            		logger.log(Level.INFO, "In first check ...");
//            		validationResult = FormValidation.warning(res.get("ErrMsg").toString());
//            	}else{
//            		logger.log(Level.INFO, "In second check ...");
//            		validationResult = FormValidation.ok(res.get("msg").toString());
//            	}
//            }else{
//            	validationResult = FormValidation.warning("GIT Pull was unsuccessful.");
//            }
//            
//        	}else{
//        		validationResult = FormValidation.ok(" ");
//        	}
//            
//        	return validationResult;
//        }
        
        @JavaScriptMethod
        public JSONObject performGitpull(String URLConnectionString,String username,String password,String gitPull,String project,String subProject,String testMode)
        {
        	logger.log(Level.INFO, "performGitpull called -");
        	JSONObject status = new JSONObject();
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		String temp = "Specify Netstorm URL Connection, Username and Password";
        		status.put("msg", temp);
        		status.put("color", "#CC0000");
        		return status;
        	}
        	
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
            JSONObject res = netstormConnectionManger.pullObjectsFromGit();
            logger.log(Level.INFO, "performGitpull: project before ..."+project);
            if(res != null && !res.isEmpty()){
            	if(!res.get("ErrMsg").toString().equals("")){
            		logger.log(Level.INFO, "performGitpull: In first check ...");
            		String tmp = res.get("ErrMsg").toString();
            		status.put("msg", tmp);
            		status.put("color", "#C4A000");
            	}else{
            		logger.log(Level.INFO, "performGitpull: In second check ...");
            		String tm = res.get("msg").toString();
            		status.put("msg", tm);
            		status.put("color", "#159537");
            	}
            }else{
            	status.put("msg", "GIT Pull was unsuccessful.");
            	status.put("color", "#C4A000");
            }
            
        	return status;
        }

        /**
         *
         * @param URLConnectionString
         * @param username
         * @param password
         * @return
         */
        public FormValidation doTestNetstormConnection(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password) {

        	
            FormValidation validationResult;
            
            
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
            
            StringBuffer errMsg = new StringBuffer();
            
            if(netstormConnectionManger.testNSConnection(errMsg))
              validationResult = FormValidation.ok("Successfully Connected");
            else
             validationResult = FormValidation.warning("Cannot Connect to NetStorm due to :" + errMsg);
                       
            return validationResult;
        }
        
        public FormValidation doPullObjectsFromGit(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password) {

        	
            FormValidation validationResult;
            StringBuffer errMsg = new StringBuffer();
                        
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
            JSONObject res = netstormConnectionManger.pullObjectsFromGit();
            logger.log(Level.INFO, "res in doPullObjectsFromGit -"+res);
            if(res != null && !res.isEmpty()){
            	if(!res.get("ErrMsg").toString().equals("")){
            		logger.log(Level.INFO, "In first check ...");
            		validationResult = FormValidation.warning(res.get("ErrMsg").toString());
            	}else{
            		logger.log(Level.INFO, "In second check ...");
            		validationResult = FormValidation.ok(res.get("msg").toString());
            	}
            }else{
            	validationResult = FormValidation.warning("GIT Pull was unsuccessful.");
            }
            doFillProjectItems(URLConnectionString,username,password);
//            ArrayList<String> projectList = netstormConnectionManger.getProjectList(errMsg);
            return validationResult;
        }
        
        public FormValidation doTestGitConfiguration(@QueryParameter("protocol") String protocol,@QueryParameter("repoIp") String repoIp,@QueryParameter("repoPort") String repoPort, @QueryParameter("repoPath") String repoPath,@QueryParameter("repoUsername") String repoUserName, @QueryParameter("repoPassword") String repoPassword,@QueryParameter("username") String username,@QueryParameter("password") String password,@QueryParameter("URLConnectionString") String URLConnectionString) {
        	FormValidation validationResult;
        	
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password first ...");
        		return validationResult;
        	}
        	else if(protocol.equals("") || protocol.equals("NA") || protocol.equals(" ") || protocol == null){
        		validationResult = FormValidation.warning("Protocol can not be empty");
        		return validationResult;
        	}
        	else if(repoIp.equals("")||repoIp.equals("NA")||repoIp.equals(" ")||repoIp == null){
        		validationResult = FormValidation.warning("Repository IP can not be empty");
        		return validationResult;
        	}
        	else if(repoPort.equals("")||repoPort.equals("NA")||repoPort.equals(" ")||repoPort == null){
        		validationResult = FormValidation.warning("Repository Port can not be empty");
        		return validationResult;
        	}
        	else if(repoPath.equals("")||repoPath.equals("NA")||repoPath.equals(" ")||repoPath == null){
        		validationResult = FormValidation.warning("Repository Path can not be empty");
        		return validationResult;
        	}
        	else if(repoUserName.equals("")||repoUserName.equals("NA")||repoUserName.equals(" ")||repoUserName == null){
        		validationResult = FormValidation.warning("Repository username can not be empty");
        		return validationResult;
        	}
        	else if(repoPassword.equals("")||repoPassword.equals("NA")||repoPassword.equals(" ")||repoPassword == null){
        		validationResult = FormValidation.warning("Repository password can not be empty");
        		return validationResult;
        	}
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
        	JSONObject res = netstormConnectionManger.checkGitConfiguration(protocol.toLowerCase(),repoIp,repoPort,repoPath,repoUserName,repoPassword,"NA");
        	if(res != null && !res.isEmpty()){
        		if(res.get("errMsg").toString().equals("")){
        			validationResult = FormValidation.ok(res.get("msg").toString());
        		}else if(!res.get("errMsg").toString().equals("")){
        			validationResult = FormValidation.warning(res.get("errMsg").toString());
        		}else{
        			validationResult = FormValidation.warning("GIT configuration test failed.");
        		}
        	}else{
        		validationResult = FormValidation.warning("GIT configuration test failed.");
        	}
        	return validationResult;
        }
        
        public FormValidation doSaveGitConfiguration(@QueryParameter("protocol") String protocol,@QueryParameter("repoIp") String repoIp,@QueryParameter("repoPort") String repoPort, @QueryParameter("repoPath") String repoPath,@QueryParameter("repoUsername") String repoUserName, @QueryParameter("repoPassword") String repoPassword,@QueryParameter("username") String username,@QueryParameter("password") String password,@QueryParameter("URLConnectionString") String URLConnectionString) {
        	FormValidation validationResult;
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password first ...");
        		return validationResult;
        	}
        	else if(protocol.equals("") || protocol.equals("NA") || protocol.equals(" ") || protocol == null){
        		validationResult = FormValidation.warning("Protocol can not be empty");
        		return validationResult;
        	}
        	else if(repoIp.equals("")||repoIp.equals("NA")||repoIp.equals(" ")||repoIp == null){
        		validationResult = FormValidation.warning("Repository IP can not be empty");
        		return validationResult;
        	}
        	else if(repoPort.equals("")||repoPort.equals("NA")||repoPort.equals(" ")||repoPort == null){
        		validationResult = FormValidation.warning("Repository Port can not be empty");
        		return validationResult;
        	}
        	else if(repoPath.equals("")||repoPath.equals("NA")||repoPath.equals(" ")||repoPath == null){
        		validationResult = FormValidation.warning("Repository Path can not be empty");
        		return validationResult;
        	}
        	else if(repoUserName.equals("")||repoUserName.equals("NA")||repoUserName.equals(" ")||repoUserName == null){
        		validationResult = FormValidation.warning("Repository username can not be empty");
        		return validationResult;
        	}
        	else if(repoPassword.equals("")||repoPassword.equals("NA")||repoPassword.equals(" ")||repoPassword == null){
        		validationResult = FormValidation.warning("Repository password can not be empty");
        		return validationResult;
        	}
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
        	String res = netstormConnectionManger.saveGitConfiguration(protocol.toLowerCase(), repoIp, repoPort, repoPath, repoUserName, repoPassword, "NA");
        	logger.log(Level.INFO, "res.............."+res);
        	validationResult=FormValidation.ok(res);
        	
        	return validationResult;
        }
           
        public synchronized ListBoxModel doFillProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password)
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
          {
            models.add("---Select Project ---");   
            return models;
          }  
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
         
          ArrayList<String> projectList = objProject.getProjectList(errMsg);
          
          //IF project list is found null
          if(projectList == null || projectList.size() == 0)
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          for(String project : projectList)
            models.add(project);
          
          return models;
        }
        
        
        // for baseline dropdown...
        public synchronized ListBoxModel doFillBaselineTypeItems()
        {
        	ListBoxModel models = new ListBoxModel();
            models.add("Select Baseline");
            models.add("All");
            models.add("Baseline1");
            models.add("Baseline2");
            models.add("Baseline3");
            
            return models;
        }
       
        public synchronized ListBoxModel doFillSubProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("project") final String project )
        {
        	
          ListBoxModel models = new ListBoxModel();
          
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || project == null || project.trim().equals(""))
          {
            models.add("---Select SubProject ---");   
            return models;
          }  
            
          if(project.trim().equals("---Select Project ---"))
          {
            models.add("---Select SubProject ---");   
            return models;
          } 
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> subProjectList = connection.getSubProjectList(errMsg, project);
          
          if(subProjectList == null || subProjectList.size() == 0)
          {
            models.add("---Select SubProject ---");   
            return models;
          }
          
          for(String subProject : subProjectList)
          {
            models.add(subProject);
          }
             
          return models;
        }
        
        public synchronized ListBoxModel doFillScenarioItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("project") final String project, @QueryParameter("subProject") final String subProject , @QueryParameter("testMode") final String testMode )
        {
        	        	
          ListBoxModel models = new ListBoxModel();
         
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || project == null || project.trim().equals("") || subProject == null || subProject.trim().equals(""))
          {
            models.add("---Select Profile ---");   
            return models;
          }  
          
          if(project.trim().equals("---Select Project ---") || subProject.trim().equals("---Select SubProject ---"))
          {
            models.add("---Select SubProject ---");   
            return models;
          } 
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> scenariosList = connection.getScenarioList(errMsg, project, subProject, testMode);
          
          if(scenariosList == null || scenariosList.size() == 0)
          {
            models.add("---Select Scenarios ---");   
            return models;
          }
          
          for(String scenarios : scenariosList)
          {
            models.add(scenarios);
          }
          
          return models;
        }
        
        public ListBoxModel doFillTestModeItems()
        {
	   
           ListBoxModel model = new ListBoxModel();
           model.add("Scenario", "N");
           model.add("Test Suite" , "T");
           
           return model;
        }
    }
}
