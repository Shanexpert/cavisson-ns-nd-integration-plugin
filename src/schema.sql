drop table IF EXISTS TestCase;
drop table IF EXISTS RunProfile;
drop table IF EXISTS UserProfile;
drop table IF EXISTS SessionProfile;
drop table IF EXISTS SessionTable;
drop table IF EXISTS PageTable;
drop table IF EXISTS TransactionTable;
drop table IF EXISTS URLTable;
drop table IF EXISTS RecordedServerTable;
drop table IF EXISTS ActualServerTable;
drop table IF EXISTS SessionRecord;
drop table IF EXISTS TransactionRecord;
drop table IF EXISTS PageRecord;
drop table IF EXISTS URLRecord;

create table TestCase (
	TestIndex int,
	TestName VARCHAR (128),
	TestType VARCHAR(64),
	--RunProfileIndex smallint,
	--RunProfileIndex int,
	WanEnv bool,
	Target int,
	NumProc smallint,
	RampupRate int,
	ProgressMsecs int,
	RunLength int,
	IdleSeconds int,
	SslPct smallint,
	KaPct smallint,
	NumKa smallint,
	MeanThink int,
	MedianThink int,
	VarThink int,
	ThinkMode VARCHAR(32),
	ReuseMode VARCHAR(32),
	UserRateMode VARCHAR(32),
	RampupMode VARCHAR(32),
	CleanupMsecs int,
	MaxConPerUser smallint,
	SessRecording VARCHAR(128),
	HealthMon bool,
	GuessNumber int,
	GuessProbability CHAR(1),
	StablizeSucc smallint,
	StablizeTries smallint,
	StablizeRun smallint,
	SLA_Metric VARCHAR(4096),
	StartTime int,
	EndTime int 
);

create table RunProfile (
	TestIndex int,
	--RunProfileIndex smallint,
	RunProfileIndex int,
	GroupNum int,
	--RunProfileName VARCHAR(128),
	--UserProfileIndex smallint,
	UserProfileIndex int,
	--SessionProfileIndex smallint,
	SessionProfileIndex int,
	Pct smallint
);

create table UserProfile (
	TestIndex int,
	--UserProfileIndex smallint,
	UserProfileIndex int,
	UserProfileName VARCHAR(128),
	UpType VARCHAR(32),
	--ValueIndex smallint,
	ValueIndex int,
	Pct smallint,
	Value VARCHAR (128)
);


create table SessionProfile (
	TestIndex int,
	--SessionProfileIndex smallint,
	SessionProfileIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	Pct smallint,
	SessionProfileName VARCHAR(128)
);

create table SessionTable (
	TestIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	SessionName VARCHAR(256)
);

create table PageTable (
	TestIndex int,
	PageIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	--TransactionIndex smallint,
	--TransactionIndex int,
	PageName VARCHAR(4096)
);

create table TransactionTable (
	TestIndex int,
	--TransactionIndex smallint,
	TransactionIndex int,
	TransactionName VARCHAR(256)
);
	
create table URLTable (
	TestIndex int,
	UrlIndex int,
	PageIndex int,
	UrlName VARCHAR(4096)
);

create table RecordedServerTable (
	TestIndex int,
	ServerGroup int,
	ServerIndex int,
	ServerName VARCHAR(128),
	ServerPort int,
	SelectionAgenda bool,
	ServerType smallint
);

create table ActualServerTable (
	TestIndex int,
	ServerGroup int,
	ServerName VARCHAR(128),
	ServerPort int,
	ServerLocation VARCHAR(128)
);

create table SessionRecord (
	TestIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	SessionInstance int,	
	--RunProfileIndex smallint,
	RunProfileIndex int,    --This is actually the scenario group index
	ChildIndex smallint,
	--AccessIndex smallint,
	AccessIndex int,
	--LocationIndex smallint,
	LocationIndex int,
	--BrowserIndex smallint,
	BrowserIndex int,
	--FreIndex smallint,
	FreIndex int,
	--MachineIndex smallint,
	MachineIndex int,
	StartTime int,
	EndTime int,
	ThinkDuration int,
	Status smallint
);

create table TransactionRecord (
	TestIndex int,
	--TransactionIndex smallint,
	TransactionIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	SessionInstance int,	
	ChildIndex smallint,
	StartTime int,
	EndTime int,
	ThinkDuration int,
	Status smallint
);
	
	
create table PageRecord (
	TestIndex int,
	--PageIndex smallint,
	PageIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	SessionInstance int,	
	--TransactionIndex smallint,
	TransactionIndex int,
	ChildIndex smallint,
	StartTime int,
	EndTime int,
	Status smallint
);
	
create table URLRecord (
	TestIndex int,
	UrlIndex int,
	--SessionIndex smallint,
	SessionIndex int,
	SessionInstance int,	
	--TransactionIndex smallint,
	TransactionIndex int,
	--PageIndex smallint,
	PageIndex int,
	ChildIndex smallint,
	FetchedFrom smallint,
	HTTSReqReused bool,
	HTTSResReused bool,
	UrlType bool,
	StartTime int,
	DnsStartTime int,
	DnsEndTime int,
	ConnectStartTime int,
	ConnectDoneTime int,
	SSLHandshakeDone int,
	WriteCompleTime int,
	FirstByteRcdTime int,
	RequestCompletedTime int,
	RenderingTime int,
	EndTime int,
	HttpResponseCode smallint,
	HttpPayloadBytesSent int,
	AppBytesSent int,
	EthBytesSent int,
	HttpPayloadBytesRcd int,
	AppBytesRcd int,
	EthBytesRcd int,
	CompletionMode smallint,
	Status smallint,
	ContentVerificationCode smallint,
	ConnectionType smallint,
	Retries smallint
);

CREATE USER anil;

GRANT ALL ON TestCase TO anil;
GRANT ALL ON RunProfile TO anil;
GRANT ALL ON UserProfile TO anil;
GRANT ALL ON SessionProfile TO anil;
GRANT ALL ON SessionTable TO anil;
GRANT ALL ON PageTable TO anil;
GRANT ALL ON TransactionTable TO anil;
GRANT ALL ON URLTable TO anil;
GRANT ALL ON RecordedServerTable TO anil;
GRANT ALL ON ActualServerTable TO anil;
GRANT ALL ON SessionRecord TO anil;
GRANT ALL ON TransactionRecord TO anil;
GRANT ALL ON PageRecord TO anil;
GRANT ALL ON URLRecord TO anil;

CREATE USER cavisson;
CREATE USER netstorm;
CREATE USER root;

GRANT ALL ON TestCase TO cavisson;
GRANT ALL ON RunProfile TO cavisson;
GRANT ALL ON UserProfile TO cavisson;
GRANT ALL ON SessionProfile TO cavisson;
GRANT ALL ON SessionTable TO cavisson;
GRANT ALL ON PageTable TO cavisson;
GRANT ALL ON TransactionTable TO cavisson;
GRANT ALL ON URLTable TO cavisson;
GRANT ALL ON RecordedServerTable TO cavisson;
GRANT ALL ON ActualServerTable TO cavisson;
GRANT ALL ON SessionRecord TO cavisson;
GRANT ALL ON TransactionRecord TO cavisson;
GRANT ALL ON PageRecord TO cavisson;
GRANT ALL ON URLRecord TO cavisson;
GRANT ALL ON SEQUENCE hibernate_sequence to cavisson;

ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON tables TO netstorm;
