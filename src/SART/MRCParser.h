#pragma once
#pragma once

#include <fstream>
#include <thread>
#include <vector>
#include <string>

using std::string;
using std::vector;

void smrc(std::string filepath, float* data, int dim[3], int ISPG = 1, float min = 0, float max = 0, float mean = 0);

class MRCParser
{

public:

	int load_original(string fileName);
	//void exportHeader(QString filename);

	//Returns size of given axis
	//dimensions(0) returns size x and so on
	int dimensions(int d)
	{
		switch (d)
		{
		case 0:
			return this->nx;
			break;
		case 1:
			return this->ny;
			break;
		case 2:
			return this->nz;
			break;
		default:
			return 0;
			break;
		}
	}

	float* getData()
	{
		return this->fdata;
	}

	//Stores dmin, dmax, dmean in
	//min max mean
	void getmmm(float& min, float& max, float& mean)
	{
		min = this->dmin;
		max = this->dmax;
		mean = this->dmean;
	}

private:
	int nx, ny, nz; //Dimensions
	int mode;
	int nxstart, nystart, nzstart;
	int mx, my, mz;
	float cellsx, cellsy, cellsz;
	float alpha, beta, gamma;
	int mapc, mapr, maps;
	float dmin, dmax, dmean; //Min, max, and mean values from data
	int spacegroup;
	int extheader;
	float x, y, z;
	int machst;
	float rms;
	string exttyp;
	int version;
	vector<string> labels;

	float* fdata; //Pointer to the MRC's data
	int fdataLength;
};

