/*
Minetest-c55
Copyright (C) 2010 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef CLIENT_HEADER
#define CLIENT_HEADER

#ifndef SERVER

#include "connection.h"
#include "environment.h"
#include "common_irrlicht.h"
#include "jmutex.h"
#include <ostream>

class ClientNotReadyException : public BaseException
{
public:
	ClientNotReadyException(const char *s):
		BaseException(s)
	{}
};

class Client;

class ClientUpdateThread : public SimpleThread
{
	Client *m_client;

public:

	ClientUpdateThread(Client *client):
			SimpleThread(),
			m_client(client)
	{
	}

	void * Thread();
};

struct IncomingPacket
{
	IncomingPacket()
	{
		m_data = NULL;
		m_datalen = 0;
		m_refcount = NULL;
	}
	IncomingPacket(const IncomingPacket &a)
	{
		m_data = a.m_data;
		m_datalen = a.m_datalen;
		m_refcount = a.m_refcount;
		if(m_refcount != NULL)
			(*m_refcount)++;
	}
	IncomingPacket(u8 *data, u32 datalen)
	{
		m_data = new u8[datalen];
		memcpy(m_data, data, datalen);
		m_datalen = datalen;
		m_refcount = new s32(1);
	}
	~IncomingPacket()
	{
		if(m_refcount != NULL){
			assert(*m_refcount > 0);
			(*m_refcount)--;
			if(*m_refcount == 0){
				if(m_data != NULL)
					delete[] m_data;
				delete m_refcount;
			}
		}
	}
	/*IncomingPacket & operator=(IncomingPacket a)
	{
		m_data = a.m_data;
		m_datalen = a.m_datalen;
		m_refcount = a.m_refcount;
		(*m_refcount)++;
		return *this;
	}*/
	u8 *m_data;
	u32 m_datalen;
	s32 *m_refcount;
};

// TODO: Remove this. It is not used as supposed.
class LazyMeshUpdater
{
public:
	LazyMeshUpdater(Environment *env)
	{
		m_env = env;
	}
	~LazyMeshUpdater()
	{
		/*
			TODO: This could be optimized. It will currently
			double-update some blocks.
		*/
		for(core::map<v3s16, bool>::Iterator
				i = m_blocks.getIterator();
				i.atEnd() == false; i++)
		{
			v3s16 p = i.getNode()->getKey();
			m_env->updateMeshes(p);
		}
		m_blocks.clear();
	}
	void add(v3s16 p)
	{
		m_blocks.insert(p, true);
	}
private:
	Environment *m_env;
	core::map<v3s16, bool> m_blocks;
};

class Client : public con::PeerHandler
{
public:
	/*
		NOTE: Every public method should be thread-safe
	*/
	Client(
			IrrlichtDevice *device,
			const char *playername,
			JMutex &range_mutex,
			s16 &viewing_range_nodes,
			bool &viewing_range_all
			);
	
	~Client();
	/*
		The name of the local player should already be set when
		calling this, as it is sent in the initialization.
	*/
	void connect(Address address);
	/*
		returns true when
			m_con.Connected() == true
			AND m_server_ser_ver != SER_FMT_VER_INVALID
		throws con::PeerNotFoundException if connection has been deleted,
		eg. timed out.
	*/
	bool connectedAndInitialized();
	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	// Called from updater thread
	// Returns dtime
	float asyncStep();

	void ProcessData(u8 *data, u32 datasize, u16 sender_peer_id);
	// Returns true if something was received
	bool AsyncProcessPacket(LazyMeshUpdater &mesh_updater);
	bool AsyncProcessData();
	void Send(u16 channelnum, SharedBuffer<u8> data, bool reliable);

	//TODO: Remove
	bool isFetchingBlocks();

	// Pops out a packet from the packet queue
	IncomingPacket getPacket();

	void groundAction(u8 action, v3s16 nodepos_undersurface,
			v3s16 nodepos_oversurface, u16 item);
	void clickObject(u8 button, v3s16 blockpos, s16 id, u16 item);

	void sendSignText(v3s16 blockpos, s16 id, std::string text);
	void sendInventoryAction(InventoryAction *a);
	
	void updateCamera(v3f pos, v3f dir);
	
	// Returns InvalidPositionException if not found
	MapNode getNode(v3s16 p);
	// Returns InvalidPositionException if not found
	//void setNode(v3s16 p, MapNode n);

	// Returns InvalidPositionException if not found
	//f32 getGroundHeight(v2s16 p);
	// Returns InvalidPositionException if not found
	//bool isNodeUnderground(v3s16 p);

	// Note: The players should not be exposed outside
	// Return value is valid until client is destroyed
	//Player * getLocalPlayer();
	// Return value is valid until step()
	//core::list<Player*> getPlayers();
	v3f getPlayerPosition();

	void setPlayerControl(PlayerControl &control);
	
	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool getLocalInventoryUpdated();
	// Copies the inventory of the local player to parameter
	void getLocalInventory(Inventory &dst);
	// TODO: Functions for sending inventory editing commands to
	//       server
	
	// Gets closest object pointed by the shootline
	// Returns NULL if not found
	MapBlockObject * getSelectedObject(
			f32 max_d,
			v3f from_pos_f_on_map,
			core::line3d<f32> shootline_on_map
	);

	// Prints a line or two of info
	void printDebugInfo(std::ostream &os);

	//s32 getDayNightIndex();
	u32 getDayNightRatio();

	//void updateSomeExpiredMeshes();
	
	void setTempMod(v3s16 p, NodeMod mod)
	{
		JMutexAutoLock envlock(m_env_mutex);
		assert(m_env.getMap().mapType() == MAPTYPE_CLIENT);
		v3s16 blockpos = ((ClientMap&)m_env.getMap()).setTempMod(p, mod);
		m_env.getMap().updateMeshes(blockpos, m_env.getDayNightRatio());
	}
	void clearTempMod(v3s16 p)
	{
		JMutexAutoLock envlock(m_env_mutex);
		assert(m_env.getMap().mapType() == MAPTYPE_CLIENT);
		v3s16 blockpos = ((ClientMap&)m_env.getMap()).clearTempMod(p);
		m_env.getMap().updateMeshes(blockpos, m_env.getDayNightRatio());
	}

	float getAvgRtt()
	{
		JMutexAutoLock lock(m_con_mutex);
		con::Peer *peer = m_con.GetPeerNoEx(PEER_ID_SERVER);
		if(peer == NULL)
			return 0.0;
		return peer->avg_rtt;
	}
	
private:
	
	// Virtual methods from con::PeerHandler
	void peerAdded(con::Peer *peer);
	void deletingPeer(con::Peer *peer, bool timeout);
	
	void ReceiveAll();
	void Receive();
	
	void sendPlayerPos();
	// This sends the player's current name etc to the server
	void sendPlayerInfo();

	float m_packetcounter_timer;
	float m_delete_unused_sectors_timer;
	float m_connection_reinit_timer;
	float m_avg_rtt_timer;
	float m_playerpos_send_timer;

	ClientUpdateThread m_thread;
	
	// NOTE: If connection and environment are both to be locked,
	// environment shall be locked first.

	Environment m_env;
	JMutex m_env_mutex;
	
	con::Connection m_con;
	JMutex m_con_mutex;

	/*core::map<v3s16, float> m_fetchblock_history;
	JMutex m_fetchblock_mutex;*/

	core::list<IncomingPacket> m_incoming_queue;
	JMutex m_incoming_queue_mutex;

	IrrlichtDevice *m_device;

	v3f camera_position;
	v3f camera_direction;
	
	// Server serialization version
	u8 m_server_ser_ver;

	float m_step_dtime;
	JMutex m_step_dtime_mutex;

	// This is behind m_env_mutex.
	bool m_inventory_updated;

	core::map<v3s16, bool> m_active_blocks;

	PacketCounter m_packetcounter;
	
	// Received from the server. 0-23999
	MutexedVariable<u32> m_time_of_day;
	
	// 0 <= m_daynight_i < DAYNIGHT_CACHE_COUNT
	//s32 m_daynight_i;
	//u32 m_daynight_ratio;
};

#endif // !SERVER

#endif // !CLIENT_HEADER

