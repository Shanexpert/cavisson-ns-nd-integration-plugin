package com.cavisson.jenkins;

import java.util.ArrayList;

public class VirtualServiceDetails {
	private String error;
	private String error_code;
	private ServicesDetails data;

	public VirtualServiceDetails(String error, String error_code, ServicesDetails data) {
		super();
		this.error = error;
		this.error_code = error_code;
		this.data = data;
	}

	public String getError() {
		return error;
	}

	public void setError(String error) {
		this.error = error;
	}

	public String getError_code() {
		return error_code;
	}

	public void setError_code(String error_code) {
		this.error_code = error_code;
	}

	public ServicesDetails getData() {
		return data;
	}

	public void setData(ServicesDetails data) {
		this.data = data;
	}

	class ServicesDetails {
		private String testSuiteName;
		private ArrayList<ServiceData> services;

		public ServicesDetails(String testSuiteName, ArrayList<ServiceData> services) {
			super();
			this.testSuiteName = testSuiteName;
			this.services = services;
		}

		public String getTestSuiteName() {
			return testSuiteName;
		}

		public void setTestSuiteName(String testSuiteName) {
			this.testSuiteName = testSuiteName;
		}

		public ArrayList<ServiceData> getServices() {
			return services;
		}

		public void setServices(ArrayList<ServiceData> services) {
			this.services = services;
		}
	}

	class ServiceData {
		private String name;
		private String url;
		private String status;
		private String templates;
		private boolean activated;
		private String error;

		public ServiceData(String name, String url, String status, String templates, String error) {
			super();
			this.name = name;
			this.url = url;
			this.status = status;
			this.templates = templates;
			this.error = error;
		}

		public String getName() {
			return name;
		}

		public void setName(String name) {
			this.name = name;
		}

		public String getUrl() {
			return url;
		}

		public void setUrl(String url) {
			this.url = url;
		}

		public String getStatus() {
			return status;
		}

		public void setStatus(String status) {
			this.status = status;
		}

		public String getTemplates() {
			return templates;
		}

		public void setTemplates(String templates) {
			this.templates = templates;
		}

		public String getError() {
			return error;
		}

		public void setError(String error) {
			this.error = error;
		}

		public boolean isActivated() {
			return activated;
		}

		public void setActivated(boolean activated) {
			this.activated = activated;
		}
		
	}
}
