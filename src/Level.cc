#include <iostream>
#include <string>
#include <vector>

#include "Level.hh"
#include "common.hh"

using namespace std;


/*
Level::Level(){
	m_dcache = NULL;
	m_icache = NULL;
	
	m_level = -1;
	m_system = NULL;
	m_isUnified = true;
}*/

Level::Level(int id_core, std::vector<ConfigCache> configs, Hierarchy* system) : m_IDcore(id_core), m_system(system){
	assert(configs.size()  >  0);
	
	m_isUnified = true;
	m_printStats = configs[0].m_printStats;
	
	m_dcache = new HybridCache(m_IDcore, false, configs[0].m_size, configs[0].m_assoc , \
					configs[0].m_blocksize, configs[0].m_nbNVMways, configs[0].m_policy, this);
	
	m_dcache->setPrintState(m_printStats);
	m_icache = NULL;	
	if(configs.size() == 2){
		m_isUnified = false;
		m_icache = new HybridCache(m_IDcore, true, configs[1].m_size, configs[1].m_assoc , \
					configs[1].m_blocksize, configs[1].m_nbNVMways, configs[1].m_policy, this);
		m_icache->setPrintState(m_printStats);
	}
}

Level::~Level()
{
	delete m_dcache;
	if(!m_isUnified)
		delete m_icache;	
}

void
Level::handleAccess(Access element)
{
	if(element.isInstFetch() && !m_isUnified)
		return m_icache->handleAccess(element);	
	else
		return m_dcache->handleAccess(element);		
}

bool
Level::lookup(Access element)
{
	if(element.isInstFetch() && !m_isUnified)
		return m_icache->lookup(element);
	else 
		return m_dcache->lookup(element);	
}

void
Level::deallocate(uint64_t addr)
{
	m_dcache->deallocate(addr);
	if(!m_isUnified)
		m_icache->deallocate(addr);
}

void
Level::signalDeallocate(uint64_t addr)
{
	if(m_IDcore == -1)
		m_system->L1sdeallocate(addr);
}


void
Level::finishSimu()
{
	m_dcache->finishSimu();
	if(!m_isUnified)
		m_icache->finishSimu();
}

void
Level::sendInvalidation(uint64_t addr, bool toInstCache)
{
	if(toInstCache && !m_isUnified)
		m_icache->signalWB(addr, true);
	else
		m_dcache->signalWB(addr, true);
}

bool
Level::receiveInvalidation(uint64_t addr)
{
		return m_icache->receiveInvalidation(addr) || m_dcache->receiveInvalidation(addr);
}

void 
Level::signalWB(uint64_t addr,  bool isDirty, bool isKept)
{
	m_system->signalWB(addr, isDirty, isKept, m_IDcore);
}
void 
Level::handleWB(uint64_t addr, bool isDirty)
{
	m_dcache->handleWB(addr, isDirty);
	if(!m_isUnified)
		m_icache->handleWB(addr, isDirty);
}


void
Level::startWarmup()
{
	m_dcache->startWarmup();
	if(!m_isUnified)
		m_icache->startWarmup();
}

void
Level::stopWarmup()
{
	m_dcache->stopWarmup();
	if(!m_isUnified)
		m_icache->stopWarmup();
}



CacheEntry* 
Level::getEntry(uint64_t addr)
{
	CacheEntry* entry = m_dcache->getEntry(addr);
	
	if(!m_isUnified && entry == NULL)
		return m_icache->getEntry(addr);
	
	return entry;
}


void
Level::printResults(std::ostream& out)
{
	out << "************************************************" << endl;
//	out << "Core n°" << m_IDcore << endl;
	if(m_isUnified)
		m_dcache->printResults(out);
	else
	{
//		out << "Data Cache " << endl;
		m_dcache->printResults(out);
		out << "******" << endl;
//		out << "Instruction Cache " << endl;
		m_icache->printResults(out);
	}
}

void
Level::printConfig(std::ostream& out)
{
	out << "************************************************" << endl;
//	out << "Core n°" << m_IDcore << endl;
	if(m_isUnified)
		m_dcache->printConfig(out);
	else
	{
//		out << "Data Cache " << endl;
		m_dcache->printConfig(out);
		out << "******" << endl;
//		out << "Instruction Cache " << endl;
		m_icache->printConfig(out);
	}
}

void
Level::openNewTimeFrame()
{
	m_dcache->openNewTimeFrame();
	if(!m_isUnified)
		m_icache->openNewTimeFrame();
}


void
Level::resetPrefetchFlag(uint64_t block_addr)
{
	m_dcache->resetPrefetchFlag(block_addr);
}

bool
Level::isPrefetchBlock(uint64_t block_addr)
{
	return m_dcache->isPrefetchBlock(block_addr);
}


void
Level::print(std::ostream& out)
{
	printResults(out);
}

