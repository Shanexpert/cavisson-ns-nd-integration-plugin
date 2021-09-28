#ifndef NS_TEST_INIT_STAT_H
#define NS_TEST_INIT_STAT_H

#include "ns_common.h"
//stagenames
#define INITIALIZATION "Initialization"               // ns-nc-nd-nvsm-gen
#define VAL_TEST_DATA "Scenario Parsing"              // ns-nc-nd-nvsm-gen
#define CREATE_DB_TABLES "Database Creation"          // ns-nc-nd-nvsm
#define CREATE_TEST_RUN_FILES "Copy Scripts"          // ns-nc-nd-nvsm-gen
#define CREATE_GEN_TEST_DATA "Generator Validation"   // nc-nvsm
#define UPLOAD_GEN_DATA "Upload Generator Data"       // nc-nvsm
#define MONITOR_SETUP "Monitor Initialization"        // ns-nc-nd-nvsm
#define NDC_SETUP "Netdiagnostics Initialization"     // ns-nc-nvsm
#define START_LOAD_GEN "CVM Initialization"               // ns-nc-nd-nvsm-gen
#define GOAL_DISCOVERY "Goal Discovery"
#define GOAL_STABILIZE "Goal Stabilize"

//Index of stages for NS
#define NS_INITIALIZATION        0
#define NS_GEN_VALIDATION        1
#define NS_SCENARIO_PARSING      2
#define NS_DB_CREATION           3
#define NS_COPY_SCRIPTS          4
#define NS_MONITOR_SETUP         5
#define NS_GOAL_DISCOVERY        6
#define NS_GOAL_STABILIZE        7
#define NS_DIAGNOSTICS_SETUP     8 
#define NS_UPLOAD_GEN_DATA       9
#define NS_START_INST            10

//filenames
#define F_INITIALIZATION "_initialization"
#define F_VAL_TEST_DATA "_scenarioValidation"
#define F_CREATE_DB_TABLES "_databaseCreation"
#define F_CREATE_TEST_RUN_FILES "_copyScripts"
#define F_CREATE_GEN_TEST_DATA "_generatorValidation"
#define F_UPLOAD_GEN_DATA "_uploadGeneratorData"
#define F_MONITOR_SETUP "_monitorInit"
#define F_NETDIAGNOSTICS_SETUP "_netdiagnosticsInit"
#define F_START_LOAD_GEN "_startInstance"
#define F_GOAL_DISCOVERY "_startGoalDiscovery"
#define F_GOAL_STABILIZE "_startStabilizeRun"

#define DESC_INITIALIZATION "Argument parsing and scenario preprocessing"
#define DESC_VAL_TEST_DATA "Scenario and script validation"
#define DESC_CREATE_DB_TABLES "Database verification and creation of DB tables" 
#define DESC_CREATE_TEST_RUN_FILES "Copying script and creating links (Using separate thread)"
#define DESC_CREATE_GEN_TEST_DATA "Creation of scripts, scenario and parameters for generators"
#define DESC_UPLOAD_GEN_DATA "Transferring script, scenario, parameters to generators"
#define DESC_MONITOR_SETUP "Making connection for monitors"
#define DESC_NETDIAGNOSTICS_SETUP "Connecting to netdiagnostics server"
#define DESC_START_LOAD_GEN "Creating and initializing Cavisson Virtual Machine (CVM)"
#define DESC_GOAL_DISCOVERY  "Starting target(session rate) discovery"
#define DESC_GOAL_STABILIZE "Verifying discovered target(session rate)"

#define DELTA_INIT_STAGE_ENTRIES 9
#define MAX_STAGE_LINE_LENGTH 1024

//TIS (TestInitializationStatus) flags
#define TIS_RUNNING  1
#define TIS_FINISHED 2
#define TIS_ERROR    3

typedef struct testInitStage {
  int stageLog_fd;
  char stageName[64];
  char stageDesc[1024];
  char stageFile[64];
  char stageStartDtTime[32];
  time_t stageStartTime;
  int stageDuration;
  char stageStatus;
} TestInitStageEntry;

extern TestInitStageEntry *testInitStageTable;
extern char testInitPath_buf[512];
extern char g_test_init_stage_id;
#ifndef CAV_MAIN
extern char g_sorted_conf_file[MAX_SCENARIO_LEN];
#else
extern __thread char g_sorted_conf_file[MAX_SCENARIO_LEN];
#endif
extern pid_t g_parent_pid;

//Functions
extern void end_stage(short,char,char *, ...);
extern void init_stage(short);
extern void write_summary_file(short,char *);
extern void write_log_file(short,char *, ...);
extern void write_test_init_header_file(char *);
extern void update_summary_desc(short, char *);
extern void create_test_init_stages();
extern void open_test_init_files();
extern void rem_invalid_stage_files();
extern void write_init_stage_files(short);
extern void set_scenario_type();
extern void append_summary_file(short, char *, FILE **);

#endif
