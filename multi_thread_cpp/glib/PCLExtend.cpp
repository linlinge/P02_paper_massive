#include "PCLExtend.h"
#define MAX2(A,B) ((A)>(B) ? (A):(B))
#define MAX3(A,B,C) MAX2(MAX2(A,B),C)
#define MIN2(A,B) ((A)<(B) ? (A):(B))
#define MIN3(A,B,C) MIN2(MIN2(A,B),C)
double ComputeMeanDistance(const pcl::PointCloud<PointType>::ConstPtr cloud)
{
	double res = 0.0;
	int n_points = 0;
	int nres;
	std::vector<int> indices(2);
	std::vector<float> sqr_distances(2);
	pcl::search::KdTree<PointType> tree;
	tree.setInputCloud(cloud);

	for (size_t i = 0; i < cloud->size(); ++i)
	{
		if (!std::isfinite((*cloud)[i].x))
		{
			continue;
		}
		nres = tree.nearestKSearch(i, 2, indices, sqr_distances);
		if (nres == 2)
		{
			res += sqrt(sqr_distances[1]);
			++n_points;
		}
	}
	if (n_points != 0)
	{
		res /= n_points;
	}
	return res;
}


double ComputeMaxDistance(const pcl::PointCloud<PointType>::ConstPtr cloud)
{
	double rst = 0.0;
	int nres;
	std::vector<int> indices(2);
	std::vector<float> sqr_distances(2);
	pcl::search::KdTree<PointType> tree;
	tree.setInputCloud(cloud);

	for (size_t i = 0; i < cloud->size(); ++i)
	{
		if (!std::isfinite((*cloud)[i].x))
		{
			continue;
		}
		nres = tree.nearestKSearch(i, 2, indices, sqr_distances);
		if (nres == 2)
		{
			double dist_tmp=sqrt(sqr_distances[1]);
			rst = rst>dist_tmp ? rst:dist_tmp;
		}
	}

	return rst;
}

vector<double> StatisticNearestDistance(const pcl::PointCloud<PointType>::ConstPtr cloud)
{
	int n_points = 0;
	int nres;
	std::vector<int> indices(2);
	std::vector<float> sqr_distances(2);
	vector<double> rst;
	pcl::search::KdTree<PointType> tree;
	tree.setInputCloud(cloud);

	for (size_t i = 0; i < cloud->size(); ++i)
	{ 
		if (!std::isfinite((*cloud)[i].x))
		{
			continue;
		}
		nres = tree.nearestKSearch(i, 2, indices, sqr_distances);
		if (nres == 2)
		{			
			rst.push_back(sqr_distances[1]);
			++n_points;
		}
	}	
	
	return rst;
}

void TransformPointCloud(pcl::PointCloud<PointType>::Ptr cloud, pcl::PointCloud<PointType>::Ptr cloud_tf,Eigen::Affine3f tf)
{
	cloud_tf->points.resize(cloud->points.size());
	
	#pragma omp parallel for
	for(int i=0;i<cloud->points.size();i++)
	{
		Eigen::Vector3f v1(cloud->points[i].x,cloud->points[i].y,cloud->points[i].z);		
		Eigen::Vector3f v2=tf*v1;
		cloud_tf->points[i].x=v2(0,0);
		cloud_tf->points[i].y=v2(1,0); 
		cloud_tf->points[i].z=v2(2,0);
	}
}

double GetCellSize(pcl::PointCloud<PointType>::Ptr cloud, int level)
{
	PointType pmin,pmax;
	pcl::getMinMax3D(*cloud,pmin,pmax);
	double cellsize=MAX3(abs(pmax.x-pmin.x),abs(pmax.y-pmin.y),abs(pmax.z-pmax.z));
	return cellsize/pow(2,level);
}
double GetBoxMin(pcl::PointCloud<PointType>::Ptr cloud)
{
	PointType pmin,pmax;
    pcl::getMinMax3D(*cloud,pmin,pmax);
	double vmin=MIN3(abs(pmax.x-pmin.x),abs(pmax.y-pmin.y),abs(pmax.z-pmin.z));
	return vmin;
}

vector<int> LoOP(pcl::PointCloud<PointType>::Ptr cloud_in, int K, double thresh)
{
	pcl::PointCloud<PointType>::Ptr cloud_out(new pcl::PointCloud<PointType>);
	pcl::search::KdTree<PointType>::Ptr kdtree(new pcl::search::KdTree<PointType>);
	kdtree->setInputCloud(cloud_in);
	vector<double> sigma;
	vector<double> plof;
	vector<double> rst_LoOP_;
    double nplof=0;
    // Resize Scores	
	sigma.resize(cloud_in->points.size());
	plof.resize(cloud_in->points.size());
	rst_LoOP_.resize(cloud_in->points.size());

	// Step 01: Calculate sigma
	#pragma omp parallel for
	for(int i=0;i<cloud_in->points.size();i++){
		// find k-nearest neighours
		vector<int> idx(K+1);
		vector<float> dist(K+1);
		kdtree->nearestKSearch (i, K+1, idx, dist);
		// cout<<cloud->points[i]<<endl;
		double sum=0;
		for(int j=1;j<K+1;j++){
			sum+=dist[j];
		}
		sum=sum/K;
		sigma[i]=sqrt(sum);
	}
	
	// Step 02: calculate mean
	double mean=0;
	#pragma omp parallel for
	for (int i = 0; i < cloud_in->points.size(); i++){        
        vector<int> idx(K+1);
		vector<float> dist(K+1);
		kdtree->nearestKSearch (cloud_in->points[i], K+1, idx, dist);
        double sum = 0;
        for (int j = 1; j < K+1; j++)
          sum += sigma[idx[j]];
        sum /= K;
        plof[i] = sigma[i] / sum  - 1.0f;				
        mean += plof[i] * plof[i];
    }
	nplof=sqrt(mean/cloud_in->points.size());	

	// Step 03: caculate score
	#pragma omp parallel for
	for(int i=0;i<cloud_in->points.size();i++){
		double value = plof[i] / (nplof * sqrt(2.0f));
		// rst_.records_[i].item1_=value;

        double dem = 1.0 + 0.278393 * value;
        dem += 0.230389 * value * value;
        dem += 0.000972 * value * value * value;
        dem += 0.078108 * value * value * value * value;
        double op = std::max(0.0, 1.0 - 1.0 / dem);
        rst_LoOP_[i] = op;
	}

	vector<int> idx;
    for(int i=0;i<rst_LoOP_.size();i++){
        if(rst_LoOP_[i]<thresh){
			idx.push_back(i);
		}                
	}

	// pcl::io::savePLYFileBinary(path_out,*cloud_out);
	return idx;
}

void RecoverColor(pcl::PointCloud<PointType>::Ptr cloud_without_color,pcl::PointCloud<PointType>::Ptr cloud_with_color)
{
	pcl::search::KdTree<PointType>::Ptr kdtree(new pcl::search::KdTree<PointType>);
	kdtree->setInputCloud(cloud_with_color);

	for(int i=0;i<cloud_without_color->points.size();i++){
		vector<int> idx;
		vector<float> dist;
		kdtree->nearestKSearch(cloud_without_color->points[i],1,idx,dist);
		cloud_without_color->points[i].r=cloud_with_color->points[idx[0]].r;
		cloud_without_color->points[i].g=cloud_with_color->points[idx[0]].g;
		cloud_without_color->points[i].b=cloud_with_color->points[idx[0]].b;

		cloud_without_color->points[i].normal_x=cloud_with_color->points[idx[0]].normal_x;
		cloud_without_color->points[i].normal_y=cloud_with_color->points[idx[0]].normal_y;
		cloud_without_color->points[i].normal_z=cloud_with_color->points[idx[0]].normal_z;
	}
}

void subtract_points(pcl::PointCloud<PointType>::Ptr cloud, const vector<int>& indices, bool flag)
{
  pcl::PointIndices::Ptr fInliers (new pcl::PointIndices);
  fInliers->indices = indices;
  pcl::ExtractIndices<PointType> extract;
  extract.setInputCloud (cloud);  
  extract.setIndices (fInliers);
  if(flag==false)
  	extract.setNegative(false);
  else 
	extract.setNegative(true);
  extract.filter(*cloud);
}

void FindCorrespondingIndices(pcl::search::KdTree<PointType>::Ptr kdtree, pcl::PointCloud<PointType>::Ptr acloud, vector<int>& indices)
{
	for(int i=0;i<acloud->points.size();i++){
		vector<float> dist;
		vector<int> idx;
		kdtree->nearestKSearch(acloud->points[i],1,idx,dist);		
		indices.push_back(idx[0]);
	}
}
void idx_convert(pcl::PointCloud<PointType>::Ptr rpc, vector<int>& ridx, pcl::PointCloud<PointType>::Ptr tpc,
                 pcl::search::KdTree<PointType>::Ptr kdtree, vector<int>& tidx)
{	
	for(int i=0;i<ridx.size();i++){
		vector<int> idx;
		vector<float> dist;
		kdtree->nearestKSearch(rpc->points[ridx[i]],3,idx,dist);
		if(dist[0]<dist[1]){
			tidx.push_back(idx[0]);
		}
		else if(dist[0]==dist[1]){
			tidx.push_back(idx[0]);
			tidx.push_back(idx[1]);
		}
		else if(dist[0]==dist[1] && dist[1]==dist[2]){
			tidx.push_back(idx[0]);
			tidx.push_back(idx[1]);
			tidx.push_back(idx[2]);
		}		
		else
		{
			cout<<"idx_convert error!"<<endl;
		}		
	}
	set<int> st(tidx.begin(),tidx.end());
	tidx.clear();
	tidx.assign(st.begin(),st.end());
}

void idx_convert(pcl::PointCloud<PointType>::Ptr rpc, pcl::PointCloud<PointType>::Ptr tpc,
                 pcl::search::KdTree<PointType>::Ptr kdtree, vector<int>& tidx)
{	
	vector<int> ridx;
	ridx.resize(rpc->points.size());

	#pragma omp parallel for
	for(int i=0;i<ridx.size();i++)
		ridx[i]=i;
	
	for(int i=0;i<ridx.size();i++){
		vector<int> idx;
		vector<float> dist;
		kdtree->nearestKSearch(rpc->points[ridx[i]],3,idx,dist);
		if(dist[0]<dist[1]){
			tidx.push_back(idx[0]);
		}
		else if(dist[0]==dist[1]){
			tidx.push_back(idx[0]);
			tidx.push_back(idx[1]);
		}
		else if(dist[0]==dist[1] && dist[1]==dist[2]){
			tidx.push_back(idx[0]);
			tidx.push_back(idx[1]);
			tidx.push_back(idx[2]);
		}		
		else
		{
			cout<<"idx_convert error!"<<endl;
		}		
	}
	set<int> st(tidx.begin(),tidx.end());
	tidx.clear();
	tidx.assign(st.begin(),st.end());
}

vector<double> get_eigenvalue(pcl::PointCloud<PointType>::Ptr cloud)
{
	// Init
	vector<double> eval;
	eval.resize(3);    

	// Calculate Evec and Eval
	Eigen::Vector4f centroid;
	Eigen::Matrix3f covariance;
	pcl::compute3DCentroid(*cloud, centroid);
	pcl::computeCovarianceMatrixNormalized(*cloud, centroid, covariance);
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);	
	Eigen::Vector3f eig_val = eigen_solver.eigenvalues();
	
	// eigenvalue
	eval[0]=eig_val(0);
	eval[1]=eig_val(1);
	eval[2]=eig_val(2);
	return eval;
}

vector<double> get_length_in_each_dimension(pcl::PointCloud<PointType>::Ptr cloud)
{
	vector<double> ld;
	ld.resize(3);
	PointType minPt,maxPt;
	pcl::getMinMax3D(*cloud,minPt,maxPt);
	ld[0]=maxPt.x-minPt.x;
	ld[1]=maxPt.y-minPt.y;
	ld[2]=maxPt.z-minPt.z;
	sort(ld.begin(),ld.end());
	return ld;
}


void RemoveNan(pcl::PointCloud<PointType>::Ptr cloud)
{
	vector<int> idx_out;
	for(int i=0;i<cloud->points.size();i++){
		if((cloud->points[i].x<1000000) && (cloud->points[i].z<1000000) && (cloud->points[i].y<1000000)){			
			idx_out.push_back(i);
		}				
	}
	pcl::copyPointCloud(*cloud,idx_out,*cloud);
}


vector<float> get_size_with_AABB(pcl::PointCloud<PointType>::Ptr cloud)
{
    pcl::MomentOfInertiaEstimation<PointType> feature_extractor;
	feature_extractor.setInputCloud(cloud);
	feature_extractor.compute();

	std::vector <float> moment_of_inertia;
	std::vector <float> eccentricity;
	PointType min_point_AABB;//AABB包围盒
	PointType max_point_AABB;

	Eigen::Vector3f major_vector, middle_vector, minor_vector;
	Eigen::Vector3f mass_center;
	feature_extractor.getMomentOfInertia(moment_of_inertia);
	feature_extractor.getEccentricity(eccentricity);
	feature_extractor.getAABB(min_point_AABB, max_point_AABB);

	// get size
	vector<float> shape;
	shape.resize(3);
	shape[0]=max_point_AABB.x-min_point_AABB.x;
	shape[1]=max_point_AABB.y-min_point_AABB.y;
	shape[2]=max_point_AABB.z-min_point_AABB.z;
	sort(shape.begin(),shape.end());
	return shape;
}
vector<float> mget_size_with_AABB(pcl::PointCloud<PointType>::Ptr cloud)
{
	PointType minPts;
	PointType maxPts;		
	minPts.x=minPts.y=minPts.z=INT_MAX;
	maxPts.x=maxPts.y=maxPts.z=-INT_MAX;
	for(int i=0;i<cloud->points.size();i++){
		minPts.x=std::min(minPts.x,cloud->points[i].x);
		minPts.y=std::min(minPts.y,cloud->points[i].y);
		minPts.z=std::min(minPts.z,cloud->points[i].z);

		maxPts.x=std::max(maxPts.x,cloud->points[i].x);
		maxPts.y=std::max(maxPts.y,cloud->points[i].y);
		maxPts.z=std::max(maxPts.z,cloud->points[i].z);
	}

	// get size
	vector<float> shape;
	shape.resize(3);
	shape[0]=maxPts.x-minPts.x;
	shape[1]=maxPts.y-minPts.y;
	shape[2]=maxPts.z-minPts.z;
	sort(shape.begin(),shape.end());
	return shape;
}

vector<float> get_size_with_OBB(pcl::PointCloud<PointType>::Ptr cloud)
{
    pcl::MomentOfInertiaEstimation <PointType> feature_extractor;
	feature_extractor.setInputCloud(cloud);
	feature_extractor.compute();

	std::vector <float> moment_of_inertia;
	std::vector <float> eccentricity;
	PointType min_point_OBB;
	PointType max_point_OBB;
	PointType position_OBB;
	Eigen::Matrix3f rotational_matrix_OBB;
	float major_value, middle_value, minor_value;
	Eigen::Vector3f major_vector, middle_vector, minor_vector;
	Eigen::Vector3f mass_center;

	feature_extractor.getMomentOfInertia(moment_of_inertia);
	feature_extractor.getEccentricity(eccentricity);
	feature_extractor.getOBB(min_point_OBB, max_point_OBB, position_OBB, rotational_matrix_OBB);

	// get size
	vector<float> shape;
	shape.resize(3);
	// Eigen::Vector3f
	// shape.resize(3);
	// shape[0]=max_point_OBB.x-min_point_OBB.x;
	// shape[1]=max_point_OBB.y-min_point_OBB.y;
	// shape[2]=max_point_OBB.z-min_point_OBB.z;
	return shape;
}

double GetMEval(pcl::PointCloud<PointType>::Ptr local_cloud)
{
	V3 mevec;
	// Calculate Evec and Eval
	Eigen::Vector4f centroid;
	Eigen::Matrix3f covariance;
	pcl::compute3DCentroid(*local_cloud, centroid);
	pcl::computeCovarianceMatrixNormalized(*local_cloud, centroid, covariance);
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);	
	Eigen::Vector3f eig_val = eigen_solver.eigenvalues();
	return eig_val(0);
}