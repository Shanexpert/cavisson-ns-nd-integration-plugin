try {
  const fs = require('fs');
  const lighthouse = require('lighthouse');
  const chromeLauncher = require('chrome-launcher');
  const CONSTANTS = require('lighthouse/lighthouse-core/config/constants.js');
  const defaultConfig = require('lighthouse/lighthouse-core/config/default-config.js');

  var msg = process.argv[2];
  var profile = process.argv[3];
  var chromePath = process.argv[4];
  var chrome;
  console.log('Netstorm passed message: ', msg);
  console.log('profile', profile);

  if (chromePath) {
    console.log('chromePath - ' +  chromePath);
  }

  (async () => {
    if(!msg || !profile) {
      console.log('Required arguments are missing.');
      process.exit(-1);
    }
    let message = JSON.parse(msg);
    const cav_config  = message.params;
    let url = message.params.url;
    let download_path = "/home/cavisson/.rbu/.chrome/logs/" + profile;
    // Check if it doesn't exsist then create it. 
    if(!fs.existsSync(download_path)) {
      //create the directory. 
      fs.mkdirSync(download_path, {recursive: true, mode: '0755'});
      //check if folder created or not.
      console.log('profile - ' +  download_path + ', exists - ' + fs.existsSync(download_path));
    }
    let lh_report_filename = download_path + "/" +  message.params.lightHouse.filename + ".html";
    let lh_csv_filename = download_path + "/csv/" +  message.params.lightHouse.filename + ".csv"; 
 
    let flags = {chromeFlags: ['--headless', '--ignore-certificate-errors']};
    if (chromePath) {
      flags['chromePath'] = chromePath;
    }
    chrome = await chromeLauncher.launch(flags);
    //const chrome = await chromeLauncher.launch();
    const selectedCategories = ["performance", "accessibility", "best-practices", "seo"];
    const options = {logLevel: 'info', output: 'html', onlyCategories: selectedCategories, port: chrome.port};
    //let config = CONSTANTS.defaultSettings;
    let config = defaultConfig;
    config.settings.throttlingMethod = 'provided';
    config.settings.screenEmulation = {
      disabled: true
    };
    config.settings.emulatedUserAgent = false;

    if (cav_config.device == 'mobile') {
      config.settings.formFactor = 'mobile';
      config.settings.screenEmulation = CONSTANTS.screenEmulationMetrics.mobile;
    } else {
      config.settings.formFactor = 'desktop';
      config.settings.screenEmulation = CONSTANTS.screenEmulationMetrics.desktop;
    }

    if (cav_config.networkThrottling.enable || cav_config.cpuThrottling.enable) {
      config.settings.throttlingMethod = 'devtools';

      if (cav_config.networkThrottling.enable) {
        // set default thorttling. 
        if (config.settings.formFactor == 'mobile') {
          config.settings.throttling = CONSTANTS.throttling.mobileSlow4G;
        } else {
          config.settings.throttling = CONSTANTS.throttling.desktopDense4G;
        }

        let rL = 0, dT = 0, uT = 0;
        if(cav_config.networkThrottling.enable){
          rL = cav_config.networkThrottling.requestLatency;
          dT = cav_config.networkThrottling.downloadThroughput;
          uT = cav_config.networkThrottling.uploadThroughput;
        }

        // set only those parameters which are non zero. 
        config.settings.throttling.requestLatencyMs = rL;
        config.settings.throttling.downloadThroughputKbps = dT;
        config.settings.throttling.uploadThroughputKbps = uT;
      } else {
        // TODO: review it. 
        // Note: we can't disable it because cpuThrottling is enabled. 
        config.settings.throttling = CONSTANTS.throttling.desktopDense4G;
      } 
      
      if (cav_config.cpuThrottling.enable && cav_config.cpuThrottling.cpuSlowDownMultiplier) {
        config.settings.throttling.cpuSlowdownMultiplier = cav_config.cpuThrottling.cpuSlowDownMultiplier; 
      } else {
        config.settings.throttling.cpuSlowdownMultiplier = 1;
      }
    }

    // tune user agent. 
    if (cav_config.userAgent) {
      config.settings.emulatedUserAgent = cav_config.userAgent;
    }

    // handling for headers. 
    if (cav_config.headers && cav_config.headers.length) {
      // make a map.
      let headerMap = {};
      cav_config.headers.forEach(header => {
        headerMap['header.name']  = header.value;
      });

      config.settings.extraHeaders = headerMap;
    }

    console.log ("options =>" + JSON.stringify(config));
 
    const runnerResult = await lighthouse(url, options, config);
 
    const reportHtml = runnerResult.report;
    fs.writeFileSync(lh_report_filename, reportHtml);
 
    const formatMetricValue = (obj) => {
      if (obj && obj.numericValue) return Math.round(obj.numericValue);
      return -1;
    } 
    const formatVal = (obj) => {
      if(obj && obj.score) return Math.round(obj.score*100);
      return 0;
    }
    //dumping -1 for pwa 
    let csv = formatVal(runnerResult.lhr.categories["performance"]) + ",-1," + 
              formatVal(runnerResult.lhr.categories["accessibility"]) + "," +
              formatVal(runnerResult.lhr.categories["best-practices"]) + "," +
              formatVal(runnerResult.lhr.categories["seo"]) + "," +
              Math.round(runnerResult.lhr.audits["first-contentful-paint"].numericValue) + "," +
              Math.round(runnerResult.lhr.audits["first-meaningful-paint"].numericValue) + "," +
              Math.round(runnerResult.lhr.audits["speed-index"].numericValue) + "," +
              formatMetricValue(runnerResult.lhr.audits["first-cpu-idle"]) + "," +
              formatMetricValue(runnerResult.lhr.audits["interactive"]) + "," +
              formatMetricValue(runnerResult.lhr.audits["estimated-input-latency"]) + "," +
              formatMetricValue(runnerResult.lhr.audits["total-blocking-time"]) + "," +
              formatMetricValue(runnerResult.lhr.audits["largest-contentful-paint"]) + "," +
              runnerResult.lhr.audits["cumulative-layout-shift"].displayValue;
              // formatMetricValue(runnerResult.lhr.audits["cumulative-layout-shift"]).toFixed(3);
 
    fs.writeFileSync(lh_csv_filename, csv);
    await chrome.kill();
 
  })();
} catch(err) {
  console.log("Exception: ", err);
  if(chrome)
    chrome.kill();
  process.exit(-1);
}
if(chrome)
  chrome.kill();
