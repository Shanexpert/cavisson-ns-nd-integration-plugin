package com.cavisson.jenkins;

import hudson.EnvVars;
import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.AbstractProject;
import hudson.model.BuildListener;
import hudson.model.FreeStyleProject;
import hudson.model.Project;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.model.User;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.Builder;
import hudson.util.FormValidation;
import hudson.util.ListBoxModel;
import hudson.slaves.EnvironmentVariablesNodeProperty;
import hudson.slaves.NodeProperty;
import hudson.slaves.NodePropertyDescriptor;
import hudson.util.DescribableList;
import jenkins.model.Jenkins;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import net.sf.json.JSONObject;
import net.sf.json.*;
import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONSerializer;
import net.sf.json.JSONArray;

import com.google.auth.oauth2.AccessToken;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.ServiceAccountCredentials;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.bind.JavaScriptMethod;

public class CreateVM extends Builder implements SimpleBuildStep{

	private String INSTANCE_NAME = "";
	private String GCP_PROJECT_ID = "";
	private String IMAGE_NAME = "";
	private String ipAddress = "";
	private String cloudVendor = "";
	Map<String, String> envVarMap = null;

	private transient static final Logger logger = Logger.getLogger(NetStormBuilder.class.getName());


	@DataBoundConstructor
	public CreateVM(String INSTANCE_NAME,String GCP_PROJECT_ID, String IMAGE_NAME, String scriptType) {
		this.INSTANCE_NAME = INSTANCE_NAME;
		this.GCP_PROJECT_ID = GCP_PROJECT_ID;
		this.IMAGE_NAME = IMAGE_NAME;
		this.cloudVendor = scriptType;
	}


	public String getINSTANCE_NAME() {
		return INSTANCE_NAME;
	}



	public void setINSTANCE_NAME(String iNSTANCE_NAME) {
		INSTANCE_NAME = iNSTANCE_NAME;
	}



	public String getGCP_PROJECT_ID() {
		return GCP_PROJECT_ID;
	}



	public void setGCP_PROJECT_ID(String gCP_PROJECT_ID) {
		GCP_PROJECT_ID = gCP_PROJECT_ID;
	}



	public String getIMAGE_NAME() {
		return IMAGE_NAME;
	}

	public void setIMAGE_NAME(String iMAGE_NAME) {
		IMAGE_NAME = iMAGE_NAME;
	}


	public String getIpAddress() {
		return ipAddress;
	}


	public void setIpAddress(String ipAddress) {
		this.ipAddress = ipAddress;
	}

	public String getScriptType() {
		return cloudVendor;
	}


	public void setScriptType(String scriptType) {
		this.cloudVendor = scriptType;
	}


	@Override
	public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {

		PrintStream consoleLog = taskListener.getLogger();

		envVarMap = run instanceof AbstractBuild ? ((AbstractBuild<?, ?>) run).getBuildVariables() : Collections.<String, String>emptyMap();
		   
		File credentialsPath = new File("/tmp/key.json");
		

		/*creating google credentials using key.json file*/
		GoogleCredentials credential;
		try (FileInputStream serviceAccountStream = new FileInputStream(credentialsPath)) {
			credential = ServiceAccountCredentials.fromStream(serviceAccountStream);
		}

		Collection<String> scope = new ArrayList<String>();
		scope.add("https://www.googleapis.com/auth/cloud-platform");
		credential = credential.createScoped(scope);
		AccessToken token = credential.refreshAccessToken();
		String tokn = token.getTokenValue();

		String payload = "{'kind':'compute#instance', 'name':'" +  INSTANCE_NAME + "', 'zone': 'projects/" + GCP_PROJECT_ID + "/zones/us-central1-a', 'machineType': 'projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/machineTypes/custom-1-4096', 'displayDevice': { 'enableDisplay': 'false' }, 'metadata': { 'kind': 'compute#metadata', 'items': [] }, 'tags': { 'items': [ 'allow-789x', 'http-server', 'https-server', 'generic-ui', 'allow-all' ] }, 'disks': [ { 'kind': 'compute#attachedDisk', 'type': 'PERSISTENT', 'boot': 'true', 'mode': 'READ_WRITE', 'autoDelete': 'true', 'deviceName':'" + INSTANCE_NAME+ "', 'initializeParams': { 'sourceImage': 'projects/" + GCP_PROJECT_ID + "/global/images/" + IMAGE_NAME+ "', 'diskType': 'projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/diskTypes/pd-standard', 'diskSizeGb': '50' }, 'diskEncryptionKey': {} } ], 'canIpForward': 'false', 'networkInterfaces': [ { 'kind': 'compute#networkInterface', 'subnetwork': 'projects/" + GCP_PROJECT_ID+ "/regions/us-central1/subnetworks/default', 'accessConfigs': [ { 'kind': 'compute#accessConfig', 'name': 'External NAT', 'type': 'ONE_TO_ONE_NAT', 'networkTier': 'PREMIUM' } ], 'aliasIpRanges': [] } ], 'description': '', 'labels': {}, 'scheduling': { 'preemptible': 'false', 'onHostMaintenance': 'MIGRATE', 'automaticRestart': 'true', 'nodeAffinities': [] }, 'deletionProtection': 'false', 'reservationAffinity': { 'consumeReservationType': 'ANY_RESERVATION' }, 'serviceAccounts': [ { 'email': 'default', 'scopes': [ 'https://www.googleapis.com/auth/devstorage.read_only', 'https://www.googleapis.com/auth/logging.write', 'https://www.googleapis.com/auth/monitoring.write', 'https://www.googleapis.com/auth/servicecontrol', 'https://www.googleapis.com/auth/service.management.readonly', 'https://www.googleapis.com/auth/trace.append' ] } ], 'confidentialInstanceConfig': { 'enableConfidentialCompute': 'false' } }";

		consoleLog.println("Creating VM Instance on Google Cloud...");

		String urladd = "https://www.googleapis.com/compute/v1/projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/instances";
		logger.log(Level.INFO, "url = " + urladd);
		URL url = new URL(urladd);
		HttpURLConnection conn = (HttpURLConnection) url.openConnection();
		conn.setConnectTimeout(0);
		conn.setReadTimeout(0);
		conn.setRequestMethod("POST");
		conn.setRequestProperty("Content-Type", "application/json");
		conn.setRequestProperty("Authorization", "Bearer " + tokn);

		// String json =payload.toString();
		conn.setDoOutput(true);

		BufferedWriter httpRequestBodyWriter = new BufferedWriter(new
				OutputStreamWriter(conn.getOutputStream()));

		httpRequestBodyWriter.write(payload);
		httpRequestBodyWriter.close();

		if (conn.getResponseCode() != 200) {
			consoleLog.println("Error in creating VM.");
			logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
			throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
		}

		consoleLog.println("VM Created Successfully on Google Cloud: " + INSTANCE_NAME);

		/*Getting instances list.*/

		consoleLog.println("Fetching IP of created VM.");
		TimeUnit.SECONDS.sleep(40);

		url = new URL("https://compute.googleapis.com/compute/v1/projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/instances/" + INSTANCE_NAME);
		HttpURLConnection conn1 = (HttpURLConnection) url.openConnection();
		conn1.setConnectTimeout(0);
		conn1.setReadTimeout(0);
		conn1.setRequestMethod("GET");
		conn1.setRequestProperty("Authorization", "Bearer " + tokn);
		conn1.setDoOutput(true);
		if (conn1.getResponseCode() != 200) {
			consoleLog.println("Error in fetching IP of VM.");
			consoleLog.println("Getting Error code = " + conn1.getResponseCode());
			throw new RuntimeException("Failed : HTTP error code : "+ conn1.getResponseCode());
		}

		BufferedReader br = null;
		br = new BufferedReader(new InputStreamReader(conn1.getInputStream()));

		StringBuilder sb = new StringBuilder();
		String line;
		while ((line = br.readLine()) != null) {
			sb.append(line);
		}
		br.close();
		logger.log(Level.INFO, "result = " + sb.toString());
		JSONObject jsonResponse = (JSONObject) JSONSerializer.toJSON(sb.toString());
		if(jsonResponse != null && jsonResponse.getJSONArray("networkInterfaces") != null) {
			JSONArray arr = (JSONArray) jsonResponse.getJSONArray("networkInterfaces");
			JSONObject obj = (JSONObject) arr.get(0);
			JSONArray ar = obj.getJSONArray("accessConfigs");
			JSONObject obt = (JSONObject) ar.get(0);
			String ip = (String) obt.getString("natIP");
			consoleLog.println("IP = " + ip);
			setIpAddress(ip);
			setIpInEnv(ip);	
		}
		TimeUnit.MINUTES.sleep(13);
	}


	public void setIpInEnv(String ip) {
		try {
		Jenkins instance = Jenkins.getInstance();
		 
	       DescribableList<NodeProperty<?>, NodePropertyDescriptor> globalNodeProperties = instance.getGlobalNodeProperties();
	       List<EnvironmentVariablesNodeProperty> envVarsNodePropertyList = globalNodeProperties.getAll(EnvironmentVariablesNodeProperty.class);
	 
	       EnvironmentVariablesNodeProperty newEnvVarsNodeProperty = null;
	       EnvVars envVars = null;
	 
	       if ( envVarsNodePropertyList == null || envVarsNodePropertyList.size() == 0 ) {
	           newEnvVarsNodeProperty = new hudson.slaves.EnvironmentVariablesNodeProperty();
	           globalNodeProperties.add(newEnvVarsNodeProperty);
	           envVars = newEnvVarsNodeProperty.getEnvVars();
	       } else {
	           envVars = envVarsNodePropertyList.get(0).getEnvVars();
	       }
	       envVars.put("GCP_NETSTORM_IP", ip);
	       instance.save();
		} catch(Exception e) {
			logger.log(Level.INFO, "Exception = " + e);
		}
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
			return Messages.CreateVM_Task();
		}

	}
}
