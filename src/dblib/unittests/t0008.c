/* 
 * Purpose: Test buffering.  As of April 2005, jkl believes this test is broken. 
 * Functions: dbclrbuf dbsetopt 
 */

#include "common.h"

static char software_version[] = "$Id: t0008.c,v 1.15 2007/11/27 12:38:12 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int
main(int argc, char **argv)
{
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	DBINT last_read = -1;

	set_malloc_options();

	read_login_info(argc, argv);

	fprintf(stdout, "Start\n");
	add_bread_crumb();

	/* Fortify_EnterScope(); */
	dbinit();

	add_bread_crumb();
	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	add_bread_crumb();
	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0008");
	DBSETLHOST(login, "ntbox.dntis.ro");

	fprintf(stdout, "About to open\n");

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "25");
#else
	dbsetopt(dbproc, DBBUFFER, "25", 0);
#endif
	add_bread_crumb();

	fprintf(stdout, "creating table\n");
	dbcmd(dbproc, "create table #dblib0008 (i int not null, s char(10) not null)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 1; i < rows_to_add; i++) {
	char cmd[1024];

		sprintf(cmd, "insert into #dblib0008 values (%d, 'row %03d')", i, i);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "select\n");
	dbcmd(dbproc, "select * from #dblib0008 order by i");
	dbsqlexec(dbproc);
	add_bread_crumb();


	if (dbresults(dbproc) != SUCCEED) {
		add_bread_crumb();
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}
	add_bread_crumb();

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		add_bread_crumb();
		printf("col %d is %s\n", i, dbcolname(dbproc, i));
		add_bread_crumb();
	}

	add_bread_crumb();
	dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint);
	add_bread_crumb();
	dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr);
	add_bread_crumb();

	add_bread_crumb();

	for (i = 1; i < rows_to_add; i++) {
	char expected[1024];

		sprintf(expected, "row %03d", i);

		add_bread_crumb();

		if (i % 25 == 0) {
			dbclrbuf(dbproc, 25);
		}

		add_bread_crumb();
		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "dblib failed for %s, dbnextrow1\n", __FILE__);
			add_bread_crumb();
			dbexit();
			add_bread_crumb();

			free_bread_crumb();
			return 1;
		}
		add_bread_crumb();
		last_read = testint;
		if (testint < 1 || testint > rows_to_add) {
			fprintf(stderr, "dblib failed for %s testint = %d\n", __FILE__, (int) testint);
			exit(1);
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	if (REG_ROW == dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s, dbnextrow2\n", __FILE__);
		add_bread_crumb();
		dbexit();
		add_bread_crumb();

		free_bread_crumb();
		return 1;
	}

	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	if (last_read == rows_to_add - 1)
		printf("dblib okay for %s\n", __FILE__);
	else
		fprintf(stderr, "dblib failed for %s last_read %d\n", __FILE__, (int) last_read);
	free_bread_crumb();
	return (last_read == rows_to_add - 1) ? 0 : 1;
}
