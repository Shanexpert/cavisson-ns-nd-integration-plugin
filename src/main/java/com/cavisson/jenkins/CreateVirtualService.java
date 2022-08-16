package com.cavisson.jenkins;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

import org.apache.tools.ant.taskdefs.Cvs;
import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.StaplerRequest;

import com.cavisson.jenkins.VirtualServiceDetails.ServiceData;
import com.google.gson.Gson;

import dnl.utils.text.table.TextTable;
import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractProject;
import hudson.model.Cause;
import hudson.model.FreeStyleProject;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.Builder;
import hudson.util.Secret;
import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONObject;

public class CreateVirtualService extends Builder implements SimpleBuildStep {

	private final String config;
	private final String API = "/ProductUI/cavservices/virtualize/v4/createservices";
	public static String HOST;
	public static String TOKEN;
	private String URLPath;


	public String getUsername() {
		return username;
	}

	/*
	 * public void setUsername(String username) { this.username = username; }
	 */

	public String getPassword() {
		return password;
	}

	/*
	 * public void setPassword(String password) { this.password = password; }
	 */

	public String getHost() {
		return host;
	}

	public String getToken() {
                return token;
        }

	/*
	 * public void setHost(String host) { this.host = host; }
	 */

	public Boolean getIsstart() {
		return isstart;
	}

	/*
	 * public void setIsstart(Boolean isstart) { this.isstart = isstart; }
	 */

	private final String rrFilePath;
	private static boolean sslCotextChanged = false;
	private final String username;
	private final String password;
	private final String token;
	private final String host;
	private final Boolean isstart ;

	public String getConfig() {
		return config;
	}

	/*
	 * public void setConfig(String config) { this.config = config; }
	 */

	public String getRrFilePath() {
		return rrFilePath;
	}

	/*
	 * public void setRrFilePath(String rrFilePath) { this.rrFilePath = rrFilePath;
	 * }
	 */

	@DataBoundConstructor
	public CreateVirtualService(String config, String rrFilePath, String host, String username, String password,
			boolean isstart, String token) {
		this.config = config;
		this.rrFilePath = rrFilePath;
		this.host = host;
		this.username = username;
		this.password = password;
		this.isstart = isstart;
		this.token = token;
		if (host != null) {
			CreateVirtualService.HOST = host;
		}
		if(token != null)
			CreateVirtualService.TOKEN = token;
	}

	public static String testRunNumber = "-1";
	public static String testCycleNumber = "";
	String path = "";

	@Override
	public void perform(Run<?, ?> run, FilePath workspace, Launcher launcher, TaskListener listener)
			throws InterruptedException, IOException {
		PrintStream logg = listener.getLogger();
		try {
			if (HOST != null) {
				Cause.UserIdCause cause = run.getCause(Cause.UserIdCause.class);
				if(cause != null)
					URLPath = HOST + API + "?token="+getToken()+"&user=" +cause.getUserName().toString();
				else
					URLPath = HOST + API + "?token="+getToken()+"&user=****";
			} else {
				logg.println("[Cavisson Service Virtualsation]: Please Provide HOST");
				run.setResult(Result.FAILURE);
				return;
			}

			logg.println("URLPath : " + URLPath);
			logg.println("Config : " + config);

			String data = getHttpResponse("POST", URLPath, config);

			VirtualServiceDetails vSd = null;
			if (data != null && !data.equals("")) {
				logg.println("data : " + data);
				vSd = new Gson().fromJson(data, VirtualServiceDetails.class);
			}

			if (vSd != null && vSd.getData() != null && vSd.getData().getServices().size() > 0) {


				int fail = 0;
				int pass = 0;
				int size = vSd.getData().getServices().size();
				logg.println("[Cavisson Service Virtualsation] Total " + size
						+ " Services Created Succesfully. Below are the detail - :");

				//logg.println("Service Name		|		URL		|		#Templates");
				//logg.println("----------------------------------------------------------------------------------");
				String[] coloumn = {"Service Name","URL","Templates","Activated"};
				Object[][] Pdata = new Object[size][4];
				boolean isFail = false;
				int isFailCount = 0;
				for (int i = 0; i < size; i++) {
					ServiceData sd = vSd.getData().getServices().get(i);
					// if status is pass
					if (sd.getStatus() != null && !sd.getStatus().equals("") && sd.getStatus().equals("pass")) {
						pass++;
					}
					// if status is fail
					if (sd.getStatus() != null && !sd.getStatus().equals("") && sd.getStatus().equals("fail")) {
						fail++;
					}
					if (!sd.isActivated()) {
						isFailCount++;
						isFail = true;
					}
					for (int l = 0; l < 3; l++) {
						Pdata[i][0] = sd.getName();
						Pdata[i][1] = sd.getUrl();
						Pdata[i][2] = sd.getTemplates();
						Pdata[i][3] = (sd.isActivated()) ? "Yes":"No";
					}
				}
				TextTable tt = new TextTable(coloumn, Pdata);                                                         
				tt.printTable(logg, 0);
				logg.println("Total " + pass + " services are created successfully.");
				if (isstart && !isFail) {
					
					String SuiteName = vSd.getData().getTestSuiteName();
					if (SuiteName != null) {
						logg.println("testSuiteName" + SuiteName);
					}	
					// set suiteName
					if (SuiteName != null && !SuiteName.equals("")) {
						String[] testsuite = SuiteName.split("/");
						if (testsuite.length == 3) {
							String project = testsuite[0];
							String subProject = testsuite[1];
							String scenario = testsuite[2];
							logg.println("Starting test with test suite(" + project + "/" + subProject + "/" + scenario
									+ ")");

							logg.println("NetStorm URI: " + HOST);
							HashMap result = new HashMap();
							StringBuffer errMsg = new StringBuffer();

							// Execution test Suite
							NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(HOST,
									getUsername(), Secret.fromString(getPassword()), project, subProject, scenario, "T",
									"", "20", "system", "", true, false, "");

							result = netstormConnectionManger.startNetstormTest(errMsg, logg, "");

							NetStormBuilder nsb = new NetStormBuilder(HOST, getUsername(), getPassword(), project,
									subProject, scenario, "T", "", "20", "system", true);
							nsb.setDoNotWaitForTestCompletion(false);

							nsb.processTestResult(result, logg, workspace, run, "", netstormConnectionManger);

						}
					}
				}
				
				if(isFail) {
					logg.println("[Cavisson Service Virtualsation]: Error: '"+vSd.getData().getServices().get(0).getName()+"' and "+(isFailCount-1)+" more Service are failed to activated. Not running Smoke test.");
					run.setResult(Result.FAILURE);
				}

				if (fail > 0)
					logg.println("NOTE: " + fail + " services are failed to be created");

				run.setResult(Result.SUCCESS);

			} else {
				logg.println(
						"[Cavisson Service Virtualsation]: Error: "+data);
				logg.println(
						"[Cavisson Service Virtualsation]: No new service is created, there is no changes in artifacts ");
				run.setResult(Result.FAILURE);
			}
		} catch (

		Exception e) {
			System.out.println("Exception in writing in file - " + e);
		}

	}

	private static synchronized void updateSSLContext() {
		if (sslCotextChanged == true)
			return;
		// Create a trust manager that does not validate certificate chains
		TrustManager[] trustAllCerts = new TrustManager[] { new X509TrustManager() {
			public java.security.cert.X509Certificate[] getAcceptedIssuers() {
				return null;
			}

			public void checkClientTrusted(X509Certificate[] certs, String authType) {
			}

			public void checkServerTrusted(X509Certificate[] certs, String authType) {
			}
		} };
		// Create all-trusting host name verifier
		HostnameVerifier allHostsValid = new HostnameVerifier() {

			public boolean verify(String hostname, SSLSession session) {
				return true;
			}
		};

		try {

			// Install the all-trusting trust manager
			SSLContext sc = SSLContext.getInstance("SSL");
			sc.init(null, trustAllCerts, new java.security.SecureRandom());
			HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());

			// Install the all-trusting host verifier
			HttpsURLConnection.setDefaultHostnameVerifier(allHostsValid);
		} catch (Exception e) {
			e.printStackTrace();
		}
		sslCotextChanged = true;
	}

	// getting http response
	public static String getHttpResponse(String method, String requestUrl, String requestData) {
		String inputLine = "";
		try {

			URLConnection conn = getURLConnection(requestUrl);

			if (conn == null)
				throw new java.net.SocketTimeoutException();

			((HttpURLConnection) conn).setRequestMethod(method);
			conn.setRequestProperty("Content-Type", "application/json");

			// Send post request
			conn.setDoOutput(true);

			// set connection timeout and connection read timeout

			if (method.equals("POST")) {
				OutputStreamWriter wr1 = new OutputStreamWriter(conn.getOutputStream(), "UTF-8");

				wr1.write(requestData);
				wr1.flush();
				wr1.close();
			}

			int responseCode = ((HttpURLConnection) conn).getResponseCode();
			// get the response and put it into BufferedReader
			BufferedReader br = null;

			br = new BufferedReader(new InputStreamReader(conn.getInputStream()));

			String line;
			while ((line = br.readLine()) != null) {
				inputLine += line;
			}
			br.close();
		} catch (Exception e) {
			e.printStackTrace();
		}
		return inputLine;
	}

	public static URLConnection getURLConnection(String resourceurl) {
		URLConnection urlConn = null;
		if (sslCotextChanged == false) {
			updateSSLContext();
		}

		try {
			URL url = new URL(resourceurl);
			urlConn = (URLConnection) url.openConnection();
			// setting connection timeout and connection read timeout
		} catch (Exception e) {
			e.printStackTrace();
		}
		return urlConn;
	}

	// synchronized update SSL Context

	@Override
	public Descriptor getDescriptor() {
		return (Descriptor) super.getDescriptor();
	}

	@Extension
	public static class Descriptor extends BuildStepDescriptor<Builder> {
		public Descriptor() {
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
		public Builder newInstance(StaplerRequest req, JSONObject formData) throws FormException {
			return super.newInstance(req, formData); // To change body of overridden methods use File | Settings | File
														// Templates.
		}

		@Override
		public String getDisplayName() {
			return Messages.CreateVirtualService_Task();
		}
	}

}
