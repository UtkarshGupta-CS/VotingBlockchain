#include <iostream>
#include <string>
#include <ctime>
#include <stdlib.h>
#include <map>

#include "./lib/picosha2.h"
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using picosha2::hash256_hex_string;
using namespace std;

void voteCheck();
int currentTime();

/* 
Returns the current time in seconds
*/
int currentTime()
{
  time_t curtime;

  /*
    Note: the `tm` structure has the following definition −
    struct tm {
      int tm_sec;         // seconds,  range 0 to 59
      int tm_min;         // minutes, range 0 to 59
      int tm_hour;        // hours, range 0 to 23
      int tm_mday;        // day of the month, range 1 to 31
      int tm_mon;         // month, range 0 to 11
      int tm_year;        // The number of years since 1900
      int tm_wday;        // day of the week, range 0 to 6
      int tm_yday;        // day in the year, range 0 to 365
      int tm_isdst;       // daylight saving time
    };
    */
  struct tm *loctime;

  /* Get the current time. */
  curtime = time(NULL);

  /* Convert it to local time representation. */
  loctime = localtime(&curtime);

  int seconds = loctime->tm_hour * 60 * 60 + loctime->tm_min * 60 + loctime->tm_sec;
  return seconds;
}

class Block
{
public:
  string timeStamp;
  string prevHash;
  string hash;
  string voterId;

  void setBlock(
      int timeStamp,
      string prevHash,
      string voterId)
  {
    this->timeStamp = to_string(timeStamp);
    this->prevHash = prevHash;
    this->hash = hash256_hex_string(to_string(timeStamp) + voterId);
    this->voterId = voterId;
  }
};

/* 
Method to check whether the list of vote entries is valid or not
*/
void voteCheck()
{

  int index = 18, totalEntries = 0;
  string fetchedHash, fetchedPrevHash, fetchedVoterId1, fetchedVoterId2;

  map<string, int> checkList;

  try
  {
    sql::Driver *driver;
    sql::Connection *con;
    sql::Statement *stmt;
    sql::ResultSet *res;
    sql::PreparedStatement *pstmt;

    driver = get_driver_instance();
    con = driver->connect("tcp://127.0.0.1:3306", "root", "");
    /* Connect to the MySQL test database */
    con->setSchema("test");

    pstmt = con->prepareStatement("SELECT * FROM test where index_no = (SELECT MAX(index_no) FROM test)");
    res = pstmt->executeQuery();

    res->afterLast();
    while (res->previous())
    {
      totalEntries = res->getInt("index_no");
    }
    while (totalEntries > index)
    {
      string query1 = "SELECT * FROM test where index_no = " + to_string(index);
      pstmt = con->prepareStatement(query1);
      res = pstmt->executeQuery();

      res->afterLast();
      while (res->previous())
      {
        fetchedHash = res->getString("hash");
        fetchedVoterId1 = res->getString("voterID");
      }

      ++index;

      string query2 = "SELECT * FROM test where index_no = " + to_string(index);

      pstmt = con->prepareStatement(query2);
      res = pstmt->executeQuery();

      res->afterLast();
      while (res->previous())
      {
        fetchedPrevHash = res->getString("prevHash");
        fetchedVoterId2 = res->getString("voterID");
      }
      int index1 = index;
      if (fetchedPrevHash.compare(fetchedHash) == 0)
      {
        pair<string, int> x;
        x.first = fetchedVoterId1;
        x.second = 1;

        checkList.insert(x);
      }

      else
      {
        pair<string, int> x;
        x.first = fetchedVoterId2;
        x.second = 0;

        checkList.insert(x);
      }

      if (index == totalEntries)
      {
        pair<string, int> x;
        x.first = fetchedVoterId2;
        x.second = 1;

        checkList.insert(x);
      }
      fetchedHash = "";
      fetchedPrevHash = "";
      fetchedVoterId1 = "";
      fetchedVoterId2 = "";
    }

  }
  catch (sql::SQLException &e)
  {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
         << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

  cout << "\n Voter ID\t Valid or Invalid(1/0) \n";
  map<string, int>::iterator it = checkList.begin();
  for (it = checkList.begin(); it != checkList.end(); it++)
  {
    cout << (*it).first << "\t\t\t" << (*it).second << endl;
  }
}

int main(void)
{
  voteCheck();
  char choice = 'n';
  cout << "Enter y to vote" << endl;
  cin >> choice;
  while (choice == 'y')
  {
    try
    {
      sql::Driver *driver;
      sql::Connection *con;
      sql::Statement *stmt;
      sql::ResultSet *res;
      sql::PreparedStatement *pstmt;

      /* Create a connection */
      driver = get_driver_instance();
      con = driver->connect("tcp://127.0.0.1:3306", "root", "");
      /* Connect to the MySQL test database */
      con->setSchema("test");

      string voterId = "";
      string fetchedPrevHash = "";
      cout << "Enter voter id \n";
      cin >> voterId;
      Block obj;
      int time = currentTime();

      pstmt = con->prepareStatement("SELECT * FROM test where index_no = (SELECT MAX(index_no) FROM test) ");
      res = pstmt->executeQuery();

      res->afterLast();
      while (res->previous())
      {
        fetchedPrevHash = res->getString("hash");
      }
      delete res;

      obj.setBlock(time, fetchedPrevHash, voterId);
      cout << obj.hash;

      /* '?' is the supported placeholder syntax */
      pstmt = con->prepareStatement("INSERT INTO test(voterID,hash,prevHash,timestamp) VALUES (?,?,?,?)");
      pstmt->setString(1, obj.voterId);
      pstmt->setString(2, obj.hash);
      pstmt->setString(3, obj.prevHash);
      pstmt->setString(4, obj.timeStamp);

      pstmt->executeUpdate();

      cout << "\nEnter n to stop voting" << endl;
      cin >> choice;

      delete pstmt;
      delete con;
    }
    catch (sql::SQLException &e)
    {
      cout << "# ERR: SQLException in " << __FILE__;
      cout << "(" << __FUNCTION__ << ") on line "
           << __LINE__ << endl;
      cout << "# ERR: " << e.what();
      cout << " (MySQL error code: " << e.getErrorCode();
      cout << ", SQLState: " << e.getSQLState() << " )" << endl;
    }
  }
  cout << endl;

  //return EXIT_SUCCESS;

  return 0;
}
