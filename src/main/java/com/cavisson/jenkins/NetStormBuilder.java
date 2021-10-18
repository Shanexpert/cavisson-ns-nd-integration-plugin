package com.cavisson.jenkins;

import hudson.EnvVars;
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
import java.util.Iterator;
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
    private String profile="";
    private String script="";
    private String page="";
    private String advanceSett="";
    private String urlHeader="";
    private String hiddenBox="";
    private final boolean generateReport;
    Map<String, String> envVarMap = null;


    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String profile,String script,String page,String advanceSett,String urlHeader,String hiddenBox,String gitPull, boolean generateReport) {
    	 logger.log(Level.INFO, "inside a constructor..............gitpull -"+gitPull);
         logger.log(Level.INFO, "profile -"+profile+", advanceSett -"+advanceSett+", urlHeader -"+urlHeader+", hiddenBox -"+hiddenBox);
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
        this.profile = profile;
        this.gitPull = gitPull; 
        this.script = script;
        this.page = page;
        this.advanceSett = advanceSett;
        this.urlHeader = urlHeader;
        this.hiddenBox = hiddenBox;
        this.generateReport = generateReport;
        
        logger.log(Level.INFO, "hiddenBox -"+this.hiddenBox+", testmode ="+testMode);
    }
    
    @DataBoundConstructor
    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String profile,String script,String page,String advanceSett,String urlHeader,String hiddenBox,String gitPull, boolean generateReport, Map<String, String> envVarMap) {
    	 logger.log(Level.INFO, "inside a constructor..............gitpull -"+gitPull);
         logger.log(Level.INFO, "profile -"+profile+", advanceSett -"+advanceSett+", urlHeader -"+urlHeader+", hiddenBox -"+hiddenBox);
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
        this.profile = profile;
        this.gitPull = gitPull; 
        this.script = script;
        this.page = page;
        this.advanceSett = advanceSett;
        this.urlHeader = urlHeader;
        this.hiddenBox = hiddenBox;
        this.generateReport = generateReport;
        this.envVarMap = envVarMap;
        
        logger.log(Level.INFO, "hiddenBox -"+this.hiddenBox+", testmode ="+testMode);
    }

    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval,String profile, boolean generateReport) {
    	 logger.log(Level.INFO, "inside second constructor..............");
        
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.profile = profile; 
        this.generateReport = generateReport;
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
	
	public String getAdvanceSett() {
		return advanceSett;
	}

	public void setAdvanceSett(String advanceSett) {
		this.advanceSett = advanceSett;
	}
     
	public String getScript() {
		return script;
	}

	public void setScript(String script) {
		this.script = script;
	}

	public String getPage() {
		return page;
	}

	public void setPage(String page) {
		this.page = page;
	}

	public String getUrlHeader() {
		return urlHeader;
	}

	public void setUrlHeader(String urlHeader) {
		this.urlHeader = urlHeader;
	}

	public String getHiddenBox() {
		return hiddenBox;
	}

	public void setHiddenBox(String hiddenBox) {
		this.hiddenBox = hiddenBox;
	}
	
	public String getProfile() {
		return profile;
	}

	public void setProfile(String profile) {
		this.profile = profile;
	}
	
	public boolean isGenerateReport() {
		return generateReport;
	}

@Override
  public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {
	run.addAction(new NetStormStopAction(run));
	Boolean fileUpload = false;

	if(envVarMap == null)
       envVarMap = run instanceof AbstractBuild ? ((AbstractBuild<?, ?>) run).getBuildVariables() : Collections.<String, String>emptyMap();
   PrintStream logg = taskListener.getLogger();
   
   NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, project, subProject, scenario, testMode, baselineType, pollInterval,profile,hiddenBox,generateReport,gitPull);      
   StringBuffer errMsg = new StringBuffer();
   
//   EnvVars envVarMap;
//   if (run instanceof AbstractBuild) {
//	   envVarMap = run.getEnvironment(taskListener);
//	   envVarMap.overrideAll(((AbstractBuild<?,?>) run).getBuildVariables());
//   } else {
//	   envVarMap = new EnvVars();
//	   
//   }
   logger.log(Level.INFO, "before if check" + envVarMap.size());
    @SuppressWarnings("rawtypes")
      Set keyset = envVarMap.keySet();
    logger.log(Level.INFO, "key set = " + keyset.size());
     String path = "";
     String jobName = "";
     String automateScripts = "";
     String testsuiteName = "";
     String dataDir = "";
     String serverhost = "";
      for(Object key : keyset)
      {
        Object value = envVarMap.get(key);
       	
       	if(key.equals("JENKINS_HOME")) {
 	     path = (String)envVarMap.get(key);
       	}
       	
       	logger.log(Level.INFO, "keys loop = " + key);
       	if(key.equals("Testsuite")) {
       		testsuiteName = (String)envVarMap.get(key);
       		logger.log(Level.INFO, "Test Suite Name = " + testsuiteName);
       		String[] testsuite = testsuiteName.split("/");
       		if(testsuite.length == 3) {
       			netstormConnectionManger.setProject(testsuite[0]);
       			netstormConnectionManger.setSubProject(testsuite[1]);
       			netstormConnectionManger.setScenario(testsuite[2]);
       		} else
       		  netstormConnectionManger.setScenario(testsuiteName);
       	}
       	
       	if(key.equals("DataDirectory")) {
       	logger.log(Level.INFO, "data dir = " + (String)envVarMap.get(key));
      	  dataDir = (String)envVarMap.get(key);
      	  netstormConnectionManger.setDataDir(dataDir);
        }

        if(key.equals("Override DataDir")) {
          dataDir = (String)envVarMap.get(key);
          if(!dataDir.equals(""))
        	 netstormConnectionManger.setDataDir(dataDir);
        }
        
        if(key.equals("Server_Host")) {
            serverhost = (String)envVarMap.get(key);
          	 netstormConnectionManger.setServerHost(serverhost);
          }
          
       			
       			
        if(value instanceof String)
        {
           String envValue = (String) value;
           
           logger.log(Level.INFO, "env value = " + envValue);
           
           netstormConnectionManger.addSLAValue("1" , "2");
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
        	   String temp [] = envValue.split("_", 3);
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
           }

           else if(envValue.startsWith("EMAIL_IDS_TO")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3)
        		   netstormConnectionManger.setEmailIdTo(temp[3]);
           }
           else if(envValue.startsWith("EMAIL_IDS_CC")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3)
        		   netstormConnectionManger.setEmailIdCc(temp[3]);
           }
           else if(envValue.startsWith("EMAIL_IDS_BCC")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3)
        		   netstormConnectionManger.setEmailIdBcc(temp[3]);
           }
           
           if(envValue.equalsIgnoreCase(fileName))
           {
             fileUpload = true;
           }
        }
      }
      
      netstormConnectionManger.setAutoScript(automateScripts);
     
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
      
      String destDir = fp + "/TestSuiteReport";
	  
	  FilePath direc = new FilePath(fp.getChannel(), destDir);
	  if(direc.exists())
		  direc.deleteRecursive(); 
	  
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
          
          if(testMode.equals("T")) {
            testCycleNumber = (String) result.get("TEST_CYCLE_NUMBER");  
            if(generateReport == true)
             netstormConnectionManger.checkTestSuiteStatus(logg, fp, run);
          }
          
 
          if(result.get("ENV_NAME") != null && !result.get("ENV_NAME").toString().trim().equals(""))
            run.setDescription((String)result.get("ENV_NAME")); 
         
          
          //To set the host and user name in a file for using in other publisher.
          
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
		  
		  NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
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

	@JavaScriptMethod
	public String getAddedHeaders(){
		try{
			logger.log(Level.INFO, "getAddedHeaders called ...hiddenBox -"+this.hiddenBox);
			return this.hiddenBox;
		}catch(Exception e){
			logger.log(Level.SEVERE, "Unknown exception in getAddedHeaders.",e);
			return "";
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
        public ArrayList<String> getPulledObjects(String value,String URLConnectionString,String username,String password,String project,String subProject,String testMode,String profile){
        	try{
        		logger.log(Level.INFO,"getPulledObjects args - value :"+value+",url -"+URLConnectionString+", project -"+project+", subProject -"+subProject+",testMode -"+testMode+", profile -"+profile);
        		ArrayList<String> res = new ArrayList<String>();
        		
        		StringBuffer errMsg = new StringBuffer();
        		if(!URLConnectionString.equals("")&&!URLConnectionString.equals("NA")&&!URLConnectionString.equals(" ")&&URLConnectionString != null&&!password.equals("") && !password.equals("NA")&&!password.equals(" ") && password != null&&!username.equals("") && !username.equals("NA") && !username.equals(" ") && username != null){
        			NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
        			if(value.equals("P")){
        				logger.log(Level.INFO,"in P.......");
        				res = connection.getProjectList(errMsg,profile);
        			}else if(value.equals("SP")){
        				logger.log(Level.INFO,"in SP.......");
        				res = connection.getSubProjectList(errMsg, project,profile);
        			}else if(value.equals("S")){
        				logger.log(Level.INFO,"in S.......");
        				res = connection.getScenarioList(errMsg, project, subProject, testMode, profile);
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
        	
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
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
            
            
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
            
            StringBuffer errMsg = new StringBuffer();
            
            if(netstormConnectionManger.testNSConnection(errMsg))
              validationResult = FormValidation.ok("Successfully Connected");
            else
             validationResult = FormValidation.warning("Cannot Connect to NetStorm due to :" + errMsg);
                       
            return validationResult;
        }
        
        public FormValidation doPullObjectsFromGit(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") String profile) {

        	
            FormValidation validationResult;
            StringBuffer errMsg = new StringBuffer();
                        
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
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
            doFillProjectItems(URLConnectionString,username,password, profile);
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
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
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
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
        	String res = netstormConnectionManger.saveGitConfiguration(protocol.toLowerCase(), repoIp, repoPort, repoPath, repoUserName, repoPassword, "NA");
        	logger.log(Level.INFO, "res.............."+res);
        	validationResult=FormValidation.ok(res);
        	
        	return validationResult;
        }
        
     // method for git profiles.............
        public synchronized ListBoxModel doFillProfileItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password)
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
          {
            models.add("---Select Profile ---");   
            return models;
          }  
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          ArrayList<String> profileList = objProject.getProfileList(errMsg);
          
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
        
                 
        public synchronized ListBoxModel doFillProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter("profile") final String profile)
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || profile == null || profile.trim().equals(""))
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          if(profile.trim().equals("---Select Profile ---"))
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          ArrayList<String> projectList = objProject.getProjectList(errMsg,profile);
          
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
       
        public synchronized ListBoxModel doFillSubProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") final String profile, @QueryParameter("project") final String project )
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
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> subProjectList = connection.getSubProjectList(errMsg, project, profile);
          
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
        
        public synchronized ListBoxModel doFillScenarioItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") final String profile, @QueryParameter("project") final String project, @QueryParameter("subProject") final String subProject , @QueryParameter("testMode") final String testMode )
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
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> scenariosList = connection.getScenarioList(errMsg, project, subProject, testMode, profile);
          
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
        
        public synchronized ListBoxModel doFillScriptItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter("profile") String profile,@QueryParameter("scenario") String scenario,@QueryParameter("project") String project,@QueryParameter("subProject") String subProject,@QueryParameter("testMode") String testMode)
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          logger.log(Level.INFO, "scriptList: url -"+URLConnectionString+",username -"+username+",password -"+password);
          logger.log(Level.INFO, "scriptList: profile -"+profile+",project -"+project+",sub project -"+subProject+",testmode -"+testMode);
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
          {
            models.add("---Select Script---");   
            return models;
          }
          
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          
          JSONArray scriptList = objProject.getScriptList(profile,scenario,project,subProject,testMode);
          logger.log(Level.INFO, "scriptList size -"+scriptList.size());
          //IF page list is found null
          if(scriptList == null || scriptList.size() == 0)
          {
            models.add("---Select Profile ---");   
            return models;
          }          
          
          //Iterator<String> iterator = scriptList.iterator();
          //while(iterator.hasNext()) {
        //	  logger.log(Level.INFO, "pageList item -"+iterator.next());
        //	  String temp = iterator.next();
        //	  logger.log(Level.INFO, "temp item -"+temp);
        //	  models.add(temp);
         // }
          
	 for(int i=0;i<scriptList.size();i++){
	   logger.log(Level.INFO, "scriptList item -"+(String)scriptList.get(i));
	   models.add((String)scriptList.get(i));
	 }

          return models;
        }
        
        public synchronized ListBoxModel doFillPageItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("script") String script,@QueryParameter("profile") String profile,@QueryParameter("scenario") String scenario,@QueryParameter("project") String project,@QueryParameter("subProject") String subProject,@QueryParameter("testMode") String testMode)
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          logger.log(Level.INFO, "pageList: url -"+URLConnectionString+",username -"+username+",password -"+password);
          logger.log(Level.INFO, "pageList: profile -"+profile+",project -"+project+",sub project -"+subProject+",testmode -"+testMode);
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || script.equals("---Select Script---"))
          {
            models.add("---Select Page---");   
            return models;
          }  
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          if(!script.equals("All")){
          JSONArray pageList = objProject.getPageList(script,scenario,profile,testMode,project,subProject);
          logger.log(Level.INFO, "pageList size -"+pageList.size());
          //IF page list is found null
          if(pageList == null || pageList.size() == 0)
          {
            models.add("---Select Profile ---");   
            return models;
          }
                    
          for(int i=0;i<pageList.size();i++){
        	  logger.log(Level.INFO, "pageList item -"+(String)pageList.get(i));
        	  String temp = (String)pageList.get(i);
        	  temp = temp.replace("\"", "");
        	  logger.log(Level.INFO, "temp -"+temp);
        	  models.add(temp);
          }
          }else if(script.equals("All")){
        	  logger.log(Level.INFO, "in all check ...");
              models.add("All");        	  
          }
        
          return models;
        }
        
        public synchronized ListBoxModel doFillUrlHeaderItems()
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          models.add("Main");
    	  models.add("Inline");
    	  models.add("ALL");
    	 
    	  return models;
        }
    }
}
