#include "common.h"


static char software_version[] = "$Id: connect.c,v 1.10 2008/11/04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void init_connect(void);

static void
init_connect(void)
{
	CHKR(SQLAllocEnv, (&Environment), "S");
	CHKR(SQLAllocConnect, (Environment, &Connection), "S");
}

int
main(int argc, char *argv[])
{
	char tmp[2048];
	SQLSMALLINT len;
	int failures = 0;
	int is_freetds = 1;

	if (read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini 
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (DRIVER[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\n", DRIVER);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	printf("SQLConnect connect..\n");
	Connect();
	if (!driver_is_freetds())
		is_freetds = 0;
	Disconnect();

	if (!is_freetds) {
		printf("Driver is not FreeTDS, exiting\n");
		return 0;
	}

	/* try connect string with using DSN */
	printf("connect string DSN connect..\n");
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	CHKR(SQLDriverConnect, (Connection, NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT), "SI");
	Disconnect();

	/* try connect string using old SERVERNAME specification */
	printf("connect string SERVERNAME connect..\n");
	printf("odbcinst.ini must be configured with FreeTDS driver..\n");

	/* this is expected to work with unixODBC */
	init_connect();
	sprintf(tmp, "DRIVER=FreeTDS;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	CHKR(SQLDriverConnect, (Connection, NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT), "SIE");
	if (RetCode == SQL_ERROR) {
		printf("Unable to open data source (ret=%d)\n", RetCode);
		++failures;
	}
	Disconnect();

	/* this is expected to work with iODBC */
	init_connect();
	sprintf(tmp, "DRIVER=%s;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;", DRIVER, SERVER, USER, PASSWORD, DATABASE);
	CHKR(SQLDriverConnect, (Connection, NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT), "SIE");
	if (RetCode == SQL_ERROR) {
		printf("Unable to open data source (ret=%d)\n", RetCode);
		++failures;
	}
	Disconnect();

	/* at least one should success.. */
	if (failures > 1) {
		CheckReturn();
		exit(1);
	}

	printf("Done.\n");
	return 0;
}
