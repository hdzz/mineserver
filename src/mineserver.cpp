#include <stdlib.h>
#ifdef WIN32
  #define _CRTDBG_MAP_ALLOC
  #include <crtdbg.h>
  #include <conio.h>
#endif

#include <cstdio>
//#include <cstdlib>
#include <deque>
#include <iostream>

#include "constants.h"

#include <SocketHandler.h>
#include <ListenSocket.h>
#include "StatusHandler.h"

#include "logger.h"

#include "DisplaySocket.h"
#include "tools.h"
#include "map.h"
#include "user.h"
#include "chat.h"
#include "config.h"
#include "nbt.h"
#include "zlib/zlib.h"

static bool quit = false;

StatusHandler h;
ListenSocket<DisplaySocket> l(h);

int main(void)
{
    uint32 starttime=(uint32)time(0);
    uint32 tick=(uint32)time(0);

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
  _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
#endif

  Map::get().initMap();

  Conf::get().load(CONFIGFILE);

  //Try to load port from config
  int port=atoi(Conf::get().value("port").c_str());
  //If failed, use default
  if(port==0) port=DEFAULT_PORT;
  //Bind to port
  if (l.Bind(port))
  {
    std::cout << "Unable to Bind port!" << std::endl;
    exit(-1);
  }
  std::cout << std::endl
            << "   _____  .__  " << std::endl
            << "  /     \\ |__| ____   ____   ______ ______________  __ ___________ " << std::endl
            << " /  \\ /  \\|  |/    \\_/ __ \\ /  ___// __ \\_  __ \\  \\/ // __ \\_  __ \\" << std::endl
            << "/    Y    \\  |   |  \\  ___/ \\___ \\\\  ___/|  | \\/\\   /\\  ___/|  | \\/" << std::endl
            << "\\____|__  /__|___|  /\\___  >____  >\\___  >__|    \\_/  \\___  >__|   " << std::endl
            << "        \\/        \\/     \\/     \\/     \\/                 \\/       " << std::endl  
            << "Version " << VERSION <<" by Fador & Psoden" << std::endl << std::endl;
            

  
  h.Add(&l);
  h.Select(1,0);
  while (!quit)
  {
    h.Select(1,0);
    if(time(0)-starttime>10)
    {
      starttime=(uint32)time(0);
      //Logger::get().log("Currently " + h.GetCount()-1 + " users in!");
      std::cout << "Currently " << h.GetCount()-1 << " users in!" << std::endl;

      //If users, ping them
      if(Users.size()>0)
      {
        //0x00 package
        uint8 data=0;
        Users[0].sendAll(&data, 1);

        //Send server time (after dawn)
        uint8 data3[9]={0x04, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x0e,0x00};
        Users[0].sendAll((uint8 *)&data3[0], 9);
      }

      //Release chunks not used in MAP_RELEASE_TIME seconds
      std::vector<coord> toRelease;
      for (std::map<int, std::map<int, int> >::const_iterator it = Map::get().mapLastused.begin(); it != Map::get().mapLastused.end(); ++it)
      {
        for (std::map<int, int>::const_iterator it2 = Map::get().mapLastused[it->first].begin();it2 != Map::get().mapLastused[it->first].end(); ++it2)
        {
          if(Map::get().mapLastused[it->first][it2->first] <= time(0)-MAP_RELEASE_TIME)
          {
            coord newCoord={it->first,0, it2->first};
            toRelease.push_back(newCoord);
          }
        }
      }

      for(unsigned i=0;i<toRelease.size();i++)
      {
        coord releaseCoord=toRelease[i];
        Map::get().releaseMap(releaseCoord.x, releaseCoord.z);
      }

    }

    //Every second
    if(time(0)-tick>0)
    {
      tick=(uint32)time(0);
      //Loop users
      for(unsigned int i=0;i<Users.size();i++)
      {
        for(uint8 j=0;j<10;j++)
        {
          //Push new map data        
          Users[i].pushMap();
        }
        for(uint8 j=0;j<20;j++)
        {
          //Remove map far away
          Users[i].popMap();
        }

        if(Users[i].logged)
        {
          Users[i].logged=false;
          //Send "On Ground" signal
          char data6[2]={0x0A, 0x01};
          h.SendSock(Users[i].sock, (char *)&data6[0], 2);

          //Add (0,0) to map queue
          Users[i].addQueue(0,0);

          //Teleport player
          Users[i].teleport(0,70,0); 
          
          /*
          for(int x=-Users[i].viewDistance;x<=Users[i].viewDistance;x++)
          {
            for(int z=-Users[i].viewDistance;z<=Users[i].viewDistance;z++)
            {
              Users[i].addQueue(x,z);
            }
          }
          */

          //Spawn this user to others
          Users[i].spawnUser(0,70*32,0);
          //Spawn other users for connected user
          Users[i].spawnOthers();
        }
      }
    }
    #ifdef WIN32
    if(_kbhit())
        quit=1;
    #endif
  }

    
  Map::get().freeMap();
  l.CloseAndDelete();

  //Windows debug
  #ifdef WIN32
      _CrtDumpMemoryLeaks();
  #endif

  return EXIT_SUCCESS;
}