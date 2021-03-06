#include "object_3d_retrieval/supervoxel_segmentation.h"

#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/point_types_conversion.h>

#include <boost/graph/graph_utility.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/one_bit_color_map.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/graph/copy.hpp>

#include <unordered_map>
#include <chrono>

#include <cereal/archives/binary.hpp>

using namespace std;

/*void supervoxel_segmentation::addSupervoxelConnectionsToViewer (pcl::PointXYZRGB& supervoxel_center,
                                       vector<pcl::PointXYZRGB>& adjacent_supervoxel_centers,
                                       vector<float>& local_weights,
                                       std::string supervoxel_name,
                                       boost::shared_ptr<pcl::visualization::PCLVisualizer> & viewer) const
{
    //Iterate through all adjacent points, and add a center point to adjacent point pair
    pcl::PointCloud<pcl::PointXYZRGBA>::iterator adjacent_itr = adjacent_supervoxel_centers.begin ();
    size_t counter = 0;
    for ( ; adjacent_itr != adjacent_supervoxel_centers.end (); ++adjacent_itr)
    {
        if (!local_pairs[counter]) { // uncomment to visualize connections as well
            ++counter;
            continue;
        }
        std::string linename = supervoxel_name + std::to_string(counter);

        vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New ();
        vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New ();
        vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New ();

        vtkUnsignedCharArray *colorT = vtkUnsignedCharArray::New();
        colorT->SetName("Colors");
        colorT->SetNumberOfComponents(3); //4 components cuz of RGBA
        unsigned char red[3] = {255, 0, 0};
        unsigned char white[3] = {255, 255, 255};

        points->InsertNextPoint (supervoxel_center.data);
        points->InsertNextPoint (adjacent_itr->data);

        // Create a polydata to store everything in
        vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New ();
        // Add the points to the dataset
        polyData->SetPoints (points);
        //std::cout << "Point size: " << points->GetNumberOfPoints () << std::endl;
        //std::cout << "Remove size: " << local_pairs.size() << std::endl;
        polyLine->GetPointIds  ()->SetNumberOfIds(points->GetNumberOfPoints ());
        for(unsigned int i = 0; i < points->GetNumberOfPoints (); i++) {
            polyLine->GetPointIds ()->SetId (i,i);
            if (local_pairs[counter]) {
                colorT->InsertNextTupleValue(red); //color for point1
            }
            else {
                colorT->InsertNextTupleValue(white); //color for point0
            }
        }
        cells->InsertNextCell (polyLine);
        // Add the lines to the dataset
        polyData->SetLines (cells);
        polyData->GetCellData()->SetScalars(colorT);
        viewer->addModelFromPolyData (polyData,linename);
        ++counter;
    }
}

void supervoxel_segmentation::visualize_segmentation(Graph* g, vector<CloudT::Ptr>& clouds_out) const
{
    boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
    viewer->setBackgroundColor (0, 0, 0);

    viewer->addPointCloud (voxel_centroid_cloud, "voxel centroids");
    viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE,2.0, "voxel centroids");
    viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_OPACITY,0.95, "voxel centroids");

    viewer->addPointCloud (colored_voxel_cloud, "colored voxels");
    viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_OPACITY,0.8, "colored voxels");

    //To make a graph of the supervoxel adjacency, we need to iterate through the supervoxel adjacency multimap
    std::multimap<uint32_t,uint32_t>::iterator label_itr = supervoxel_adjacency.begin ();
    for ( ; label_itr != supervoxel_adjacency.end (); ) {
        //This function is shown below, but is beyond the scope of this tutorial - basically it just generates a "star" polygon mesh from the points given
        addSupervoxelConnectionsToViewer (supervoxel->centroid_, adjacent_supervoxel_centers, ss.str (), viewer, local_pairs);
    }

    while (!viewer->wasStopped()) {
        viewer->spinOnce (100);
    }
}*/

void supervoxel_segmentation::subsample_cloud(CloudT::Ptr& cloud_out, CloudT::Ptr& cloud_in)
{
    // these parameters are working very nicely
    pcl::ApproximateVoxelGrid<PointT> vf;
    vf.setInputCloud(cloud_in);
    vf.setLeafSize(0.005f, 0.005f, 0.005f);
    vf.filter(*cloud_out);
}

void supervoxel_segmentation::preprocess_cloud(CloudT::Ptr& cloud_out, CloudT::Ptr& cloud_in)
{
    float filter_dist = 0.02f;

    /*CloudT::Ptr cloud_constrained(new CloudT);
    // Create the filtering object
    pcl::PassThrough<PointT> pass;
    pass.setInputCloud(cloud_in);
    pass.setFilterFieldName("z");
    pass.setFilterLimits(-3.0, 3.0); // 0.0, 0.0
    //pass.setFilterLimitsNegative (true);
    pass.filter(*cloud_constrained);*/

    chrono::time_point<std::chrono::system_clock> start, intermediate, end;
    start = chrono::system_clock::now();

    CloudT::Ptr cloud_constrained(new CloudT());
    cloud_constrained->reserve(cloud_in->size());
    for (const PointT& p : cloud_in->points) {
        if (pcl::isFinite(p) && p.getVector3fMap().norm() < 3.0f) {
            cloud_constrained->push_back(p);
        }
    }

    intermediate = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = intermediate-start;

    cout << "Distance filtering took " << elapsed_seconds.count() << " seconds" << endl;

    if (do_filter) {
        // outlier removal
        pcl::RadiusOutlierRemoval<PointT> outrem;
        // build the filter
        outrem.setInputCloud(cloud_constrained);
        outrem.setRadiusSearch(filter_dist); // 0.02 // Kinect 2
        outrem.setMinNeighborsInRadius(30); // this might be a bit too much!
        // apply filter
        outrem.filter(*cloud_out);
    }
    else {
        *cloud_out += *cloud_constrained;
    }

    // This is even slower the radius outlier removal, maybe slightly better
    /*
    pcl::StatisticalOutlierRemoval<PointT> outrem;
    outrem.setInputCloud(cloud_constrained);
    outrem.setMeanK(50);
    outrem.setStddevMulThresh(1.0);
    outrem.filter(*cloud_out);
    */

    end = chrono::system_clock::now();
    elapsed_seconds = end-start;

    cout << "Cloud preprocessing took " << elapsed_seconds.count() << " seconds" << endl;
}

// this might be more useful if it takes as input two point clouds and two
// normal clouds
float supervoxel_segmentation::boundary_convexness(VoxelT::Ptr& first_supervoxel,
                                                   VoxelT::Ptr& second_supervoxel)
{
    CloudT::Ptr first_voxels = first_supervoxel->voxels_;
    NormalCloudT::Ptr first_normals = first_supervoxel->normals_;
    CloudT::Ptr second_voxels = second_supervoxel->voxels_;
    NormalCloudT::Ptr second_normals = second_supervoxel->normals_;

    Eigen::Vector3f n1 = first_supervoxel->normal_.getNormalVector3fMap();
    Eigen::Vector3f n2 = second_supervoxel->normal_.getNormalVector3fMap();

    float flat_penalty = planar_weight;
    if (acos(fabs(n1.dot(n2))) < M_PI / 8.0) {
        return flat_penalty;
    }

    float dist_threshold = 0.05;

    // TODO: seriously, use kd trees for this!!!
    pcl::KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud(second_voxels);

    float mindist = std::numeric_limits<float>::infinity();
    float count = 0.0f;
    float mean_prod = 0.0f;
    for (size_t i = 0; i < first_voxels->size(); ++i) {
        PointT point = first_voxels->at(i);
        Eigen::Vector3f normali = first_normals->at(i).getNormalVector3fMap();
        Eigen::Vector3f pe = point.getVector3fMap();
        std::vector<int> indices;
        std::vector<float> distances;
        kdtree.nearestKSearchT(point, 1, indices, distances);
        if (distances.empty()) {
            continue;
        }
        float dist = sqrt(distances[0]);
        if (dist > dist_threshold) {
            continue;
        }
        if (dist < mindist) {
            mindist = dist;
        }
        count += 1.0f;
        size_t ind = indices[0];
        Eigen::Vector3f diff = pe - second_voxels->at(ind).getVector3fMap();
        Eigen::Vector3f normalj = second_normals->at(ind).getNormalVector3fMap();
        if (acos(fabs(normali.dot(normalj))) < M_PI/8.0) {
            mean_prod += flat_penalty;
            continue;
        }
        diff.normalize();
        float convexness = -diff.dot(normalj - normali);
        if (convexness < 0) {
            mean_prod += convexness;
        }
        else {
            mean_prod += convexness;
        }
    }
    mean_prod /= count;

    return mean_prod;
}

void supervoxel_segmentation::graph_cut(vector<Graph*>& graphs_out, Graph& graph_in)
{
    using adjacency_iterator = boost::graph_traits<Graph>::adjacency_iterator;
    typename boost::property_map<Graph, boost::vertex_index_t>::type vertex_id = boost::get(boost::vertex_index, graph_in);
    typename boost::property_map<Graph, boost::edge_weight_t>::type edge_id = boost::get(boost::edge_weight, graph_in);
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, graph_in);

    // define a property map, `parities`, that will store a boolean value for each vertex.
    // Vertices that have the same parity after `stoer_wagner_min_cut` runs are on the same side of the min-cut.
    BOOST_AUTO(parities, boost::make_one_bit_color_map(boost::num_vertices(graph_in), boost::get(boost::vertex_index, graph_in)));

    // run the Stoer-Wagner algorithm to obtain the min-cut weight. `parities` is also filled in.
    float w = boost::stoer_wagner_min_cut(graph_in, boost::get(boost::edge_weight, graph_in), boost::parity_map(parities));

    cout << "1. The min-cut weight of G is " << w << ".\n" << endl;
    cout << "The threshold is " << cut_threshold << ".\n" << endl;

    // this will make it more likely to cut smaller parts
    if (w > cut_threshold) {

        // compute number of edges in cut
        size_t num_edges = 0;
        using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
        edge_iterator edge_it, edge_end;
        for (tie(edge_it, edge_end) = boost::edges(graph_in); edge_it != edge_end; ++edge_it) {
            //std::cout << *edge_it << std::endl;
            Vertex u = source(*edge_it, graph_in);
            Vertex v = target(*edge_it, graph_in);
            VertexIndex uindex = boost::get(vertex_id, u);
            VertexIndex vindex = boost::get(vertex_id, v);
            int uclass = boost::get(parities, uindex);
            int vclass = boost::get(parities, vindex);
            if (uclass != vclass) {
                ++num_edges;
            }
        }

        cout << "And we cut in " << num_edges << " places " << endl;

        if (w/float(num_edges) > cut_threshold) {
            cout << "We will not perform this cut" << endl;

            return;
        }
    }

    cout << "2. The min-cut weight of G is " << w << ".\n" << endl;

    unordered_map<VertexIndex, VertexIndex> mappings;
    VertexIndex counters[2] = {0, 0};

    graphs_out.push_back(new Graph(1));
    graphs_out.push_back(new Graph(1));
    //std::cout << "One set of vertices consists of:" << std::endl;
    bool flag;
    Edge edge;
    for (size_t i = 0; i < boost::num_vertices(graph_in); ++i) {
        int first = boost::get(parities, i);
        // iterate adjacent edges
        adjacency_iterator ai, ai_end;
        for (tie(ai, ai_end) = boost::adjacent_vertices(i, graph_in);  ai != ai_end; ++ai) {
            VertexIndex neighbor_index = boost::get(vertex_id, *ai);
            int second = boost::get(parities, neighbor_index);
            if (first == second && neighbor_index < i) {
                tie(edge, flag) = boost::edge(i, neighbor_index, graph_in);
                edge_weight_property weight = boost::get(edge_id, edge);
                if (mappings.count(i) == 0) {
                    mappings[i] = counters[first]++;
                }
                if (mappings.count(neighbor_index) == 0) {
                    mappings[neighbor_index] = counters[first]++;
                }
                tie(edge, flag) = boost::add_edge(mappings[neighbor_index], mappings[i], weight, *graphs_out[first]);

                typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name_first = boost::get(boost::vertex_name, *graphs_out[first]);
                boost::get(vertex_name_first, mappings[i]) = boost::get(vertex_name, i);
                boost::get(vertex_name_first, mappings[neighbor_index]) = boost::get(vertex_name, *ai);
            }
        }
    }

}

void supervoxel_segmentation::connected_components(vector<Graph*>& graphs_out, Graph& graph_in)
{
    // this is used to get a vertex by boost::get
    typename boost::property_map<Graph, boost::vertex_index_t>::type vertex_id = boost::get(boost::vertex_index, graph_in);
    typename boost::property_map<Graph, boost::edge_weight_t>::type edge_id = boost::get(boost::edge_weight, graph_in);
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, graph_in);
    typedef VertexIndex* Rank;
    typedef Vertex* Parent;

    std::vector<VertexIndex> rank(num_vertices(graph_in));
    std::vector<Vertex> parent(num_vertices(graph_in));

    boost::disjoint_sets<Rank, Parent> ds(&rank[0], &parent[0]);
    initialize_incremental_components(graph_in, ds);
    incremental_components(graph_in, ds);

    using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = boost::edges(graph_in); edge_it != edge_end; ++edge_it) {
        //std::cout << *edge_it << std::endl;
        Vertex u = source(*edge_it, graph_in);
        Vertex v = target(*edge_it, graph_in);
        ds.union_set(boost::get(vertex_id, u), boost::get(vertex_id, v));
    }

    Components components(parent.begin(), parent.end());
    unordered_map<VertexIndex, VertexIndex> mappings;

    bool flag;
    Edge edge;
    // Iterate through the component indices
    for (VertexIndex current_index : components) {
        graphs_out.push_back(new Graph(0));
        VertexIndex counter = 0;
        // Iterate through the child vertex indices for [current_index]
        BOOST_FOREACH (VertexIndex child_index, components[current_index]) { // what the fuck does this do????
            typename boost::graph_traits<Graph>::adjacency_iterator ai, ai_end;
            for (tie(ai, ai_end) = boost::adjacent_vertices(child_index, graph_in);  ai != ai_end; ++ai) {
                VertexIndex neighbor_index = boost::get(vertex_id, *ai);
                tie(edge, flag) = boost::edge(child_index, neighbor_index, graph_in);
                edge_weight_property weight = boost::get(edge_id, edge);
                if (neighbor_index < child_index) { // NOTE: this used to be <, remove if something does not work
                    if (mappings.count(child_index) == 0) {
                        mappings[child_index] = counter++;
                    }
                    if (mappings.count(neighbor_index) == 0) {
                        mappings[neighbor_index] = counter++;
                    }
                    tie(edge, flag) = boost::add_edge(mappings[neighbor_index], mappings[child_index], weight, *graphs_out.back());

                    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name_back = boost::get(boost::vertex_name, *graphs_out.back());
                    boost::get(vertex_name_back, mappings[child_index]) = boost::get(vertex_name, child_index);
                    boost::get(vertex_name_back, mappings[neighbor_index]) = boost::get(vertex_name, *ai);
                }
            }
        }

        if (graph_size(*graphs_out.back()) == 0) {
            delete graphs_out.back();
            graphs_out.pop_back();
        }
    }

    // construct the segmented graphs and optionally visualize input and output
    cout << "Showing main graph" << endl;
    /*visualize_boost_graph(graph_in);
    for (Graph* g : graphs_out) {
        cout << "Showing subgraph" << endl;
        visualize_boost_graph(*g);
    }*/

}

float supervoxel_segmentation::mean_graph_weight(Graph& graph_in)
{
    using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
    typename boost::property_map<Graph, boost::edge_weight_t>::type edge_id = boost::get(boost::edge_weight, graph_in);

    float weight_sum = 0.0f;

    int counter = 0;
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = boost::edges(graph_in); edge_it != edge_end; ++edge_it) {
        edge_weight_property weight = boost::get(edge_id, *edge_it);
        weight_sum += fabs(weight.m_value);
        ++counter;
    }

    return weight_sum / float(counter);
}

size_t supervoxel_segmentation::graph_size(Graph& graph_in)
{
    return boost::num_vertices(graph_in);
}

size_t supervoxel_segmentation::graph_edges_size(Graph& graph_in)
{
    return boost::num_edges(graph_in);
}

void supervoxel_segmentation::recursive_split(vector<Graph*>& graphs_out, Graph& graph_in)
{
    // to begin with, find disjoint parts
    connected_components(graphs_out, graph_in);

    if (graph_size(graph_in) < 5) {
        return;
    }

    cout << "Graphs 1 size: " << graphs_out.size() << endl;

    vector<size_t> delete_indices;

    bool changed = false;
    // then find the parts that are large enough and have a high enough split score
    size_t _graphs_out_size = graphs_out.size();
    for (size_t i = 0; i < _graphs_out_size; ++i) {
        if (graph_size(*graphs_out[i]) <= 6) {
            continue;
        }
        cout << "Graph i size: " << graph_size(*graphs_out[i]) << endl;
        // split these segments using graph cuts
        vector<Graph*> second_graphs;
        graph_cut(second_graphs, *graphs_out[i]); // 0.5 for querying segmentation
        if (!second_graphs.empty()) {
            graphs_out.insert(graphs_out.end(), second_graphs.begin(), second_graphs.end());
            delete_indices.push_back(i);
            changed = true;
        }
    }

    cout << "Graphs 2" << endl;

    if (!changed) {
        return;
    }

    cout << "Graphs 2.5" << endl;

    for (size_t i : boost::adaptors::reverse(delete_indices)) {
        delete graphs_out[i];
        graphs_out[i] = graphs_out.back();
        graphs_out.pop_back();
    }

    cout << "Graphs 3 size: " << graphs_out.size() << endl;

    delete_indices.clear();
    // for each of the resulting segments that qualify, call this function again
    _graphs_out_size = graphs_out.size();
    for (size_t i = 0; i < _graphs_out_size; ++i) {
        vector<Graph*> second_graphs;
        recursive_split(second_graphs, *graphs_out[i]);
        graphs_out.insert(graphs_out.end(), second_graphs.begin(), second_graphs.end());
        delete_indices.push_back(i);
    }

    cout << "Graphs 4 size: " << graphs_out.size() << endl;

    for (size_t i : boost::adaptors::reverse(delete_indices)) {
        delete graphs_out[i];
        graphs_out[i] = graphs_out.back();
        graphs_out.pop_back();
    }

    cout << "Graphs 5 size: " << graphs_out.size() << endl;
}

void supervoxel_segmentation::compute_voxel_clouds(vector<CloudT::Ptr>& segment_voxels, map<uint32_t, size_t>& voxel_inds,
                                                   supervoxel_map& supervoxels, float voxel_resolution, CloudT::Ptr& original)
{
    for (pair<const uint32_t, VoxelT::Ptr>& s : supervoxels) {
        voxel_inds.insert(make_pair(s.first, segment_voxels.size()));
        segment_voxels.push_back(CloudT::Ptr(new CloudT));
        *segment_voxels.back()  += *s.second->voxels_;
    }
}

/*
vector<supervoxel_segmentation::CloudT::Ptr> supervoxel_segmentation::compute_voxel_clouds(supervoxel_map& supervoxels)
{
    vector<CloudT::Ptr> voxel_clouds;
    for (pair<const uint32_t, VoxelT::Ptr>& s : supervoxels) {
        voxel_inds.insert(make_pair(s.first, segment_voxels.size()));
        segment_voxels.push_back(CloudT::Ptr(new CloudT));
        *segment_voxels.back()  += *s.second->voxels_;
    }
}
*/

vector<supervoxel_segmentation::CloudT::Ptr> supervoxel_segmentation::compute_rgb_clouds(pcl::PointCloud<pcl::PointXYZL>::Ptr& cloud_l,
                                                                                         CloudT::Ptr& cloud_in,
                                                                                         supervoxel_map& supervoxel_clusters,
                                                                                         int max_label)
{
    pcl::KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud(cloud_in);

    vector<CloudT::Ptr> rgb_clouds;
    vector<size_t> indices(max_label);
    size_t counter = 0;
    for (const std::pair<uint32_t, VoxelT::Ptr>& i : supervoxel_clusters) {
        rgb_clouds.push_back(CloudT::Ptr(new CloudT));
        //cout << "Indices size: " << indices.size() << endl;
        //cout << "Label: " << i.first-1 << endl;
        indices[i.first-1] = counter;
        ++counter;
    }
    for (const pcl::PointXYZL& l : cloud_l->points) {
        if (l.label == 0) {
            continue;
        }
        PointT p;
        p.x = l.x; p.y = l.y; p.z = l.z;
        std::vector<int> ind;
        std::vector<float> distances;
        kdtree.nearestKSearchT(p, 1, ind, distances);
        if (distances.empty()) {
            continue;
        }
        //cout << "Indices size: " << indices.size() << endl;
        //cout << "Label: " << l.label-1 << endl;
        //cout << "Clouds size: " << rgb_clouds.size() << endl;
        //cout << "Iteration: " << indices[l.label-1] << endl;
        rgb_clouds[indices[l.label-1]]->push_back(cloud_in->at(ind[0]));
    }

    return rgb_clouds;
}

supervoxel_segmentation::Graph* supervoxel_segmentation::create_supervoxel_graph(vector<CloudT::Ptr>& segments,
                                                                                 vector<CloudT::Ptr>& rgb_segments,
                                                                                 CloudT::Ptr& cloud_in,
                                                                                 NormalCloudT::Ptr* normals_in)
{
    // pre-process clouds
    CloudT::Ptr cloud(new CloudT);
    preprocess_cloud(cloud, cloud_in);

    supervoxel_map supervoxel_clusters;
    multimap<uint32_t, uint32_t> supervoxel_adjacency;

    bool use_transform = false; // false this far
    float seed_resolution = 0.2f;
    float color_importance = 0.6f;
    float spatial_importance = 0.4f;
    float normal_importance = 1.0f;

    ////////////////////////////////////////////////////////////
    ////// This is how to use supervoxels //////////////////////
    ////////////////////////////////////////////////////////////

    chrono::time_point<std::chrono::system_clock> start, end;
    start = chrono::system_clock::now();

    pcl::SupervoxelClustering<PointT> super(voxel_resolution, seed_resolution, use_transform);
    if (normals_in == NULL) {
        super.setInputCloud(cloud);
    }
    else {
        super.setInputCloud(cloud_in);
        super.setNormalCloud(*normals_in); // something
    }
    super.setColorImportance(color_importance);
    super.setSpatialImportance(spatial_importance);
    super.setNormalImportance(normal_importance);

    pcl::console::print_highlight("Extracting supervoxels!\n");
    super.extract(supervoxel_clusters);
    pcl::console::print_info("Found %d supervoxels\n", supervoxel_clusters.size ());
    //super.refineSupervoxels(3, supervoxel_clusters);

    end = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = end-start;

    cout << "Supervoxel extraction took " << elapsed_seconds.count() << " seconds" << endl;
    cout << "Original cloud size: " << cloud_in->size() << endl;

    pcl::PointCloud<pcl::PointXYZL>::Ptr label_cloud = super.getLabeledCloud();
    cout << "Labelled cloud size: " << label_cloud->size() << endl;

    rgb_segments = compute_rgb_clouds(label_cloud, cloud_in, supervoxel_clusters, super.getMaxLabel());

    /*boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer ("3D Viewer"));
    viewer->setBackgroundColor(1, 1, 1);
    pcl::visualization::PointCloudColorHandlerRGBField<PointT> rgb(cloud_in);
    viewer->addPointCloud<PointT>(cloud_in, rgb, "sample cloud");
    viewer->addPointCloud<pcl::PointXYZL>(label_cloud, "labeled voxels");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_OPACITY, 0.5, "labeled voxels");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2.0, "labeled voxels");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 0.8, "sample cloud");
    //viewer->addCoordinateSystem(1.0);
    viewer->initCameraParameters();
    while (!viewer->wasStopped()) {
        viewer->spinOnce(100);
    }*/

    // get the stuff we need from the representation
    super.getSupervoxelAdjacency(supervoxel_adjacency);
    map<uint32_t, size_t> voxel_inds;
    compute_voxel_clouds(segments, voxel_inds, supervoxel_clusters, voxel_resolution, cloud_in);

    // convert the graph to a boost graph instead
    Graph* graph = new Graph(supervoxel_clusters.size());
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, *graph);

    bool flag;
    Edge edge;
    using map_iterator = multimap<uint32_t,uint32_t>::iterator;
    map_iterator label_itr = supervoxel_adjacency.begin ();
    for ( ; label_itr != supervoxel_adjacency.end (); ) {
        //supervoxel_clusters.at(label_it->first)->centroid_
        map_iterator adjacent_itr = supervoxel_adjacency.equal_range (label_itr->first).first;
        for ( ; adjacent_itr != supervoxel_adjacency.equal_range(label_itr->first).second; ++adjacent_itr) {
            // maybe need to add a check if label_itr->first > adjacent_itr->second
            VoxelT::Ptr first_supervoxel = supervoxel_clusters.at(label_itr->first);
            VoxelT::Ptr second_supervoxel = supervoxel_clusters.at(adjacent_itr->second);
            float weight = boundary_convexness(first_supervoxel, second_supervoxel);
            edge_weight_property e = weight;
            tie(edge, flag) = boost::add_edge(voxel_inds[label_itr->first], voxel_inds[adjacent_itr->second], e, *graph);
            boost::get(vertex_name, voxel_inds[label_itr->first]) = voxel_inds[label_itr->first];
            boost::get(vertex_name, voxel_inds[adjacent_itr->second]) = voxel_inds[adjacent_itr->second];
        }
        label_itr = supervoxel_adjacency.upper_bound(label_itr->first);
    }

    return graph;
}

// add option to subsample here
void supervoxel_segmentation::visualize_segments(vector<Graph*> graphs, vector<CloudT::Ptr>& voxel_clouds)
{
    int colormap[][3] = {
        {166,206,227},
        {31,120,180},
        {178,223,138},
        {51,160,44},
        {251,154,153},
        {227,26,28},
        {253,191,111},
        {255,127,0},
        {202,178,214},
        {106,61,154},
        {255,255,153},
        {177,89,40},
        {141,211,199},
        {255,255,179},
        {190,186,218},
        {251,128,114},
        {128,177,211},
        {253,180,98},
        {179,222,105},
        {252,205,229},
        {217,217,217},
        {188,128,189},
        {204,235,197},
        {255,237,111}
    };

    CloudT::Ptr colored_cloud(new CloudT);

    pair<double, double> ratio = {0.0, 0.0};

    size_t counter = 0;
    for (Graph* g : graphs) {
        typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, *g);
        using vertex_iterator = boost::graph_traits<Graph>::vertex_iterator;

        int rc = colormap[counter%24][0];
        int gc = colormap[counter%24][1];
        int bc = colormap[counter%24][2];

        vertex_iterator vertex_iter, vertex_end;
        for (tie(vertex_iter, vertex_end) = boost::vertices(*g); vertex_iter != vertex_end; ++vertex_iter) {
            vertex_name_property name = boost::get(vertex_name, *vertex_iter);
            ratio.second++;
            if (name.m_value >= voxel_clouds.size()) {
                ratio.first++;
            }
            for (PointT p : voxel_clouds[name.m_value]->points) {
                p.r = rc;
                p.g = gc;
                p.b = bc;
                colored_cloud->push_back(p);
            }
        }
        ++counter;
    }

    cout << "Visualization wrong ratio: " << ratio.first / ratio.second << endl;

    boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
    viewer->setBackgroundColor(1, 1, 1);
    pcl::visualization::PointCloudColorHandlerRGBField<PointT> rgb(colored_cloud);
    viewer->addPointCloud<PointT>(colored_cloud, rgb, "sample cloud");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
    viewer->initCameraParameters();
    while (!viewer->wasStopped()) {
        viewer->spinOnce(100);
    }
}

// add option to subsample here
void supervoxel_segmentation::visualize_segments(vector<CloudT::Ptr>& clouds_out, bool subsample)
{
    int colormap[][3] = {
        {166,206,227},
        {31,120,180},
        {178,223,138},
        {51,160,44},
        {251,154,153},
        {227,26,28},
        {253,191,111},
        {255,127,0},
        {202,178,214},
        {106,61,154},
        {255,255,153},
        {177,89,40},
        {141,211,199},
        {255,255,179},
        {190,186,218},
        {251,128,114},
        {128,177,211},
        {253,180,98},
        {179,222,105},
        {252,205,229},
        {217,217,217},
        {188,128,189},
        {204,235,197},
        {255,237,111}
    };

    CloudT::Ptr colored_cloud(new CloudT);
    size_t counter = 0;
    for (CloudT::Ptr& c : clouds_out) {
        cout << "Cloud " << counter << " size: " << c->size() << endl;
        size_t i = colored_cloud->size();
        colored_cloud->resize(i + c->size());
        int r = colormap[counter%24][0];
        int g = colormap[counter%24][1];
        int b = colormap[counter%24][2];

        for (PointT p : c->points) {
            p.r = r;
            p.g = g;
            p.b = b;
            colored_cloud->points[i] = p;
            ++i;
        }
        ++counter;
    }

    cout << "Colored cloud size: " << colored_cloud->size() << endl;

    boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
    viewer->setBackgroundColor(1, 1, 1);
    if (subsample) {
        CloudT::Ptr subsampled_cloud(new CloudT);

        pcl::ApproximateVoxelGrid<PointT> vf;
        vf.setInputCloud(colored_cloud);
        vf.setLeafSize(0.05f, 0.05f, 0.05f);
        vf.filter(*subsampled_cloud);

        pcl::visualization::PointCloudColorHandlerRGBField<PointT> rgb(subsampled_cloud);
        viewer->addPointCloud<PointT>(subsampled_cloud, rgb, "sample cloud");
    }
    else {
        pcl::visualization::PointCloudColorHandlerRGBField<PointT> rgb(colored_cloud);
        viewer->addPointCloud<PointT>(colored_cloud, rgb, "sample cloud");
    }
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
    //viewer->addCoordinateSystem(1.0);
    viewer->initCameraParameters();
    while (!viewer->wasStopped()) {
        viewer->spinOnce(100);
    }
}

/*
void supervoxel_segmentation::create_full_segment_clouds(vector<CloudT::Ptr>& full_segments, vector<CloudT::Ptr>& supervoxel_segments,
                                                         map<size_t, size_t>& indices, vector<CloudT::Ptr>& segments,
                                                         CloudT::Ptr& cloud, vector<Graph*>& graphs)
{
    using vertex_iterator = boost::graph_traits<Graph>::vertex_iterator;

    pcl::octree::OctreePointCloudSearch<PointT> octree(3*voxel_resolution); // 2* for querying segmentation
    octree.defineBoundingBox(-10.0, 10.0, 0.0, 10.0, 10.0, 10.1);
    octree.setInputCloud(cloud);
    octree.addPointsFromInputCloud();

    size_t i = 0;
    for (Graph* g : graphs) {
        // first, form the complete voxel cloud
        typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, *g);
        CloudT::Ptr graph_voxels(new CloudT);
        vertex_iterator vertex_iter, vertex_end;
        cout << "Graph has " << graph_size(*g) << " vertices." << endl;
        for (tie(vertex_iter, vertex_end) = boost::vertices(*g); vertex_iter != vertex_end; ++vertex_iter) {
            vertex_name_property name = boost::get(vertex_name, *vertex_iter);
            *graph_voxels += *segments[name.m_value];
            indices.insert(make_pair(name.m_value, i));
        }

        pcl::octree::OctreePointCloudSearch<PointT> octree_segment(3*voxel_resolution); // 2* for querying segmentation
        octree_segment.defineBoundingBox(-10.0, 10.0, 0.0, 10.0, 10.0, 10.1);
        octree_segment.setInputCloud(graph_voxels);
        octree_segment.addPointsFromInputCloud();

        full_segments.push_back(CloudT::Ptr(new CloudT));

        vector<PointT, Eigen::aligned_allocator<PointT> > voxel_centers;
        octree_segment.getOccupiedVoxelCenters(voxel_centers);
        for (PointT& p : voxel_centers) {
            pcl::PointIndices::Ptr point_idx_data(new pcl::PointIndices());
            if (!octree.voxelSearch(p, point_idx_data->indices)) {
                continue;
            }
            CloudT cloud_p;
            pcl::ExtractIndices<PointT> extract;
            extract.setInputCloud(cloud);
            extract.setIndices(point_idx_data);
            extract.setNegative(false);
            extract.filter(cloud_p);
            *full_segments.back() += cloud_p;
        }

        ++i;
    }

    for (CloudT::Ptr& c : segments) {
        pcl::octree::OctreePointCloudSearch<PointT> octree_segment(3*voxel_resolution); // 2* for querying segmentation
        octree_segment.defineBoundingBox(-10.0, 10.0, 0.0, 10.0, 10.0, 10.1);
        octree_segment.setInputCloud(c);
        octree_segment.addPointsFromInputCloud();

        supervoxel_segments.push_back(CloudT::Ptr(new CloudT));

        vector<PointT, Eigen::aligned_allocator<PointT> > voxel_centers;
        octree_segment.getOccupiedVoxelCenters(voxel_centers);
        for (PointT& p : voxel_centers) {
            pcl::PointIndices::Ptr point_idx_data(new pcl::PointIndices());
            if (!octree.voxelSearch(p, point_idx_data->indices)) {
                continue;
            }
            CloudT cloud_p;
            pcl::ExtractIndices<PointT> extract;
            extract.setInputCloud(cloud);
            extract.setIndices(point_idx_data);
            extract.setNegative(false);
            extract.filter(cloud_p);
            *supervoxel_segments.back() += cloud_p;
        }
    }
}
*/

pair<vector<supervoxel_segmentation::CloudT::Ptr>, map<size_t, size_t> > supervoxel_segmentation::merge_connected_clouds(vector<CloudT::Ptr>& rgb_segments,
                                                                                                                         vector<Graph*> graphs)
{
    vector<CloudT::Ptr> connected_clouds;
    map<size_t, size_t> indices;

    size_t i = 0;
    for (Graph* g : graphs) {
        typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, *g);
        using vertex_iterator = boost::graph_traits<Graph>::vertex_iterator;

        connected_clouds.push_back(CloudT::Ptr(new CloudT));

        vertex_iterator vertex_iter, vertex_end;
        for (tie(vertex_iter, vertex_end) = boost::vertices(*g); vertex_iter != vertex_end; ++vertex_iter) {
            vertex_name_property name = boost::get(vertex_name, *vertex_iter);
            *connected_clouds.back() += *rgb_segments[name.m_value];
            indices.insert(make_pair(name.m_value, i));
        }

        ++i;
    }

    return make_pair(connected_clouds, indices);
}

vector<supervoxel_segmentation::Graph*> supervoxel_segmentation::color_model_split(vector<Graph*>& graphs, vector<CloudT::Ptr>& voxel_clouds)
{
    using vertex_iterator = boost::graph_traits<Graph>::vertex_iterator;

    const float mutual_color_information_weight = 0.005;//0.01;

    vector<pcl::PointCloud<pcl::PointXYZHSV>::Ptr> converted_voxels;
    for (CloudT::Ptr& c : voxel_clouds) {
        cout << "Converting voxel cloud..." << endl;
        converted_voxels.push_back(pcl::PointCloud<pcl::PointXYZHSV>::Ptr(new pcl::PointCloud<pcl::PointXYZHSV>));
        pcl::PointCloudXYZRGBtoXYZHSV(*c, *converted_voxels.back());
    }

    //vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d> > means;
    //vector<Eigen::Matrix3d, Eigen::aligned_allocator<Eigen::Matrix3d> > covs;
    vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d> > means;
    vector<Eigen::Matrix2d, Eigen::aligned_allocator<Eigen::Matrix2d> > covs;
    for (pcl::PointCloud<pcl::PointXYZHSV>::Ptr& c : converted_voxels) {
        //means.push_back(Eigen::Vector3d(0.0, 0.0, 0.0));
        means.push_back(Eigen::Vector2d(0.0, 0.0));
        for (const pcl::PointXYZHSV& p : c->points) {
            //Eigen::Vector3d hsv(p.h, p.s, p.v);
            Eigen::Vector2d hsv(p.h, p.s);
            means.back() += hsv;
        }
        means.back() /= double(c->size());

        //covs.push_back(Eigen::Matrix3d());
        covs.push_back(Eigen::Matrix2d());
        covs.back().setZero();
        for (const pcl::PointXYZHSV& p : c->points) {
            //Eigen::Vector3d hsvp(p.h, p.s, p.v);
            Eigen::Vector2d hsvp(p.h, p.s);
            hsvp -= means.back();
            covs.back() += hsvp*hsvp.transpose();
        }
        covs.back() /= double(c->size());
    }

    // iterate over all the different segments and form a color model for every segment
    vector<Graph*> new_graphs;
    vector<Graph*> deleted_graphs;
    for (Graph* g : graphs) {

        typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, *g);
        typename boost::property_map<Graph, boost::edge_weight_t>::type edge_id = boost::get(boost::edge_weight, *g);

        if (graph_size(*g) < 10) {
            new_graphs.push_back(g);
            continue;
        }

        //Eigen::Vector3d m(0.0, 0.0, 0.0);
        Eigen::Vector2d m(0.0, 0.0);
        vertex_iterator vertex_iter, vertex_end;
        double normalization = 0.0;
        for (tie(vertex_iter, vertex_end) = boost::vertices(*g); vertex_iter != vertex_end; ++vertex_iter) {
            vertex_name_property name = boost::get(vertex_name, *vertex_iter);
            m += double(voxel_clouds[name.m_value]->size())*means[name.m_value];
            normalization += voxel_clouds[name.m_value]->size();
        }
        m /= normalization;

        //Eigen::Matrix3d E;
        Eigen::Matrix2d E;
        E.setZero();
        for (tie(vertex_iter, vertex_end) = boost::vertices(*g); vertex_iter != vertex_end; ++vertex_iter) {
            vertex_name_property name = boost::get(vertex_name, *vertex_iter);
            for (const pcl::PointXYZHSV& p : converted_voxels[name.m_value]->points) {
                //Eigen::Vector3d hsvp(p.h, p.s, p.v);
                Eigen::Vector2d hsvp(p.h, p.s);
                hsvp -= m;
                E += hsvp*hsvp.transpose();
            }
        }
        E /= normalization;

        using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
        edge_iterator edge_it, edge_end;
        for (tie(edge_it, edge_end) = boost::edges(*g); edge_it != edge_end; ++edge_it) {


            Vertex u = source(*edge_it, *g);
            Vertex v = target(*edge_it, *g);
            vertex_name_property from = boost::get(vertex_name, u);
            vertex_name_property to = boost::get(vertex_name, v);

            /*auto KLD = [](const Eigen::Vector3d& m1, const Eigen::Matrix3d& E1,
                          const Eigen::Vector3d& m2, const Eigen::Matrix3d& E2)
            {
                Eigen::Matrix3d E2I = E2.inverse();
                return 0.5*(log(E2.determinant()) - log(E1.determinant()) - 3.0 +
                            (E2I*E1).trace() + (m2-m1).transpose()*E2I*(m2-m1));
            };*/
            auto KLD = [](const Eigen::Vector2d& m1, const Eigen::Matrix2d& E1,
                          const Eigen::Vector2d& m2, const Eigen::Matrix2d& E2)
            {
                Eigen::Matrix2d E2I = E2.inverse();
                return 0.5*(log(E2.determinant()) - log(E1.determinant()) - 2.0 +
                            (E2I*E1).trace() + (m2-m1).transpose()*E2I*(m2-m1));
            };

            double dist12 = KLD(means[from.m_value], covs[from.m_value], means[to.m_value], covs[to.m_value]);
            double dist1 = KLD(m, E, means[to.m_value], covs[to.m_value]);
            double dist2 = KLD(m, E, means[from.m_value], covs[from.m_value]);
            // similar to normalized mutual information with KLD: https://en.wikipedia.org/wiki/Mutual_information
            double dist = dist12 / std::min(dist1, dist2);
            if (std::isnan(dist) || std::isinf(dist)) {
                continue;
            }
            float& weight = boost::get(edge_id, *edge_it);
            weight -= mutual_color_information_weight*dist;

            cout << to.m_value << " out of " << covs.size() << endl;
            cout << "Dist: " << dist << endl;
        }

        // now we try to split the graph using these new weights
        // but we only split if the average weight is sufficiently low
        vector<Graph*> split_graph;
        recursive_split(split_graph, *g); // 0.5 for querying segmentation
        new_graphs.insert(new_graphs.end(), split_graph.begin(), split_graph.end());
        deleted_graphs.push_back(g);
    }

    for (int i = 0; i < deleted_graphs.size(); ++i) {
        delete deleted_graphs[i];
        deleted_graphs[i] = NULL;
    }

    return new_graphs;
}

// we do not need to preserve the graph, only modify the segments
// we need some way of getting at the underlying voxels here
// indices denote which segment each supervoxel belongs to
// are the segments and output of the class, in that case merge those clouds as well
// store a graph with pair<float, float> as edge property?
void supervoxel_segmentation::post_merge_convex_segments(vector<CloudT::Ptr>& merged_segments, map<size_t, size_t>& indices,
                                                         vector<CloudT::Ptr>& full_segments, Graph& graph_in)
{
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, graph_in);
    typename boost::property_map<Graph, boost::edge_weight_t>::type edge_id = boost::get(boost::edge_weight, graph_in);

    // definitions for the merged graph
    typedef boost::property<boost::edge_weight_t, pair<float, float> > edge_weight_property_dual;
    typedef boost::property<boost::vertex_name_t, size_t> vertex_name_property_dual;
    using graph_dual = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, vertex_name_property_dual, edge_weight_property_dual>;
    using vertex_dual = boost::graph_traits<graph_dual>::vertex_descriptor;
    using vertex_index_dual = boost::graph_traits<graph_dual>::vertices_size_type;
    using edge_dual = boost::graph_traits<graph_dual>::edge_descriptor;

    graph_dual graph(indices.size());

    typename boost::property_map<graph_dual, boost::vertex_name_t>::type vertex_name_dual = boost::get(boost::vertex_name, graph); // float?
    typename boost::property_map<graph_dual, boost::edge_weight_t>::type edge_id_dual = boost::get(boost::edge_weight, graph);

    bool flag;
    edge_dual dual_edge;

    using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = boost::edges(graph_in); edge_it != edge_end; ++edge_it) {
        float weight = boost::get(edge_id, *edge_it);
        Vertex u = source(*edge_it, graph_in);
        Vertex v = target(*edge_it, graph_in);
        vertex_name_property from = boost::get(vertex_name, u);
        vertex_name_property to = boost::get(vertex_name, v);
        if (indices.count(from.m_value) == 0 || indices.count(to.m_value) == 0 ||
                indices[from.m_value] == indices[to.m_value]) {
            continue;
        }
        // either there is no edge or there is already one...
        tie(dual_edge, flag) = boost::edge(indices[from.m_value], indices[to.m_value], graph);
        if (flag) {
            pair<float, float>& weights = boost::get(edge_id_dual, dual_edge); // this needs to be the mean weight instead, count associated with edge, starts at 1
            weights.first += weight; weights.second += 1.0f;
        }
        else {
            tie(dual_edge, flag) = boost::add_edge(indices[from.m_value], indices[to.m_value], make_pair(weight, 1.0f), graph);
            boost::get(vertex_name_dual, indices[from.m_value]) = indices[from.m_value];
            boost::get(vertex_name_dual, indices[to.m_value]) = indices[to.m_value];
        }
    }

    //visualize_boost_graph(graph);

    // keep a list of vertices that we need to traverse before we're done
    bool traversed = false;
    map<size_t, set<size_t> > merged_indices;
    for (size_t i = 0; i < full_segments.size(); ++i) {
        merged_indices[i].insert(i);
    }

    float threshold = 0.15f; // quite conservative

    cout << "Pre merge number of segments: " << merged_indices.size() << endl;
    cout << "Number of edges: " << boost::num_edges(graph) << endl;

    using edge_iterator_dual = boost::graph_traits<graph_dual>::edge_iterator;
    edge_iterator_dual dual_edge_it, dual_edge_end;
    while (!traversed) {
        traversed = true;
        for (tie(dual_edge_it, dual_edge_end) = boost::edges(graph); dual_edge_it != dual_edge_end; ++dual_edge_it) {
            pair<float, float> merge_weight = boost::get(edge_id_dual, *dual_edge_it);
            vertex_dual u = source(*dual_edge_it, graph);
            vertex_dual v = target(*dual_edge_it, graph);
            vertex_name_property_dual new_source = boost::get(vertex_name_dual, u);
            vertex_name_property_dual old_target = boost::get(vertex_name_dual, v);
            cout << "Weight: " << merge_weight.first / merge_weight.second << endl;
            if (merge_weight.second > 1.5 && merge_weight.first / merge_weight.second > threshold) {
                typename boost::graph_traits<graph_dual>::adjacency_iterator ai, ai_end;
                for (tie(ai, ai_end) = boost::adjacent_vertices(v, graph);  ai != ai_end; ++ai) {
                    if (*ai == u) {
                        continue;
                    }
                    tie(dual_edge, flag) = boost::edge(v, *ai, graph);
                    pair<float, float> weight = boost::get(edge_id_dual, dual_edge);
                    tie(dual_edge, flag) = boost::edge(u, *ai, graph);
                    if (flag) {
                        pair<float, float>& weights = boost::get(edge_id_dual, dual_edge);
                        weights.first += weight.first; weights.second += weight.second; // this needs to be the mean weight instead, count associated with edge, starts at 1
                    }
                    else {
                        tie(dual_edge, flag) = boost::add_edge(u, *ai, weight, graph);
                    }
                }
                // now, delete edges and vertex of v
                merged_indices[new_source.m_value].insert(merged_indices[old_target.m_value].begin(), merged_indices[old_target.m_value].end());
                merged_indices.erase(old_target.m_value);
                boost::clear_vertex(v, graph);
                boost::remove_vertex(v, graph);
                traversed = false;
                break;
            }
        }
    }

    cout << "Post merge number of segments: " << merged_indices.size() << endl;
    cout << "Number of edges: " << boost::num_edges(graph) << endl;

    auto change_index = [&indices](size_t i, size_t j) {
        for (pair<const size_t, size_t>& ind : indices) {
            if (ind.second == i) {
                ind.second = j;
            }
        }
    };

    //indices.clear();
    for (const pair<size_t, set<size_t> >& s : merged_indices) {
        if (s.second.size() > 1) {
            CloudT::Ptr merged_cloud(new CloudT);
            cout << "Post process merging " << s.second.size() << " full segments " << endl;
            for (size_t i : s.second) {
                *merged_cloud += *full_segments[i];
                change_index(i, merged_segments.size());
                cout << i << " ";
                //indices[i] = merged_segments.size(); // this does not work, since i is the convex segment index
            }
            cout << endl;
            if (merged_cloud->size() > 0) {
                merged_segments.push_back(merged_cloud);
            }
        }
        else {
            if (full_segments[s.first]->size() > 0) {
                //indices[s.first] = merged_segments.size();
                change_index(s.first, merged_segments.size());
                merged_segments.push_back(full_segments[s.first]);
            }
        }
    }
}

supervoxel_segmentation::Graph* supervoxel_segmentation::create_merged_graph(Graph& graph, vector<Graph*>& graphs,
                                                                             map<size_t, size_t>& indices,
                                                                             vector<CloudT::Ptr>& connected_clouds)
{
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name = boost::get(boost::vertex_name, graph);
    const bool add_further_edges = true;

    Graph* graph_merged = new Graph(graphs.size());
    typename boost::property_map<Graph, boost::vertex_name_t>::type vertex_name_merged = boost::get(boost::vertex_name, *graph_merged);

    bool flag;
    Edge edge;
    using edge_iterator = boost::graph_traits<Graph>::edge_iterator;
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = boost::edges(graph); edge_it != edge_end; ++edge_it) {
        Vertex u = source(*edge_it, graph);
        Vertex v = target(*edge_it, graph);
        vertex_name_property from = boost::get(vertex_name, u);
        vertex_name_property to = boost::get(vertex_name, v);
        if (indices.count(from.m_value) == 0 || indices.count(to.m_value) == 0) {
            continue;
        }
        if (indices[from.m_value] == indices[to.m_value]) {
            continue;
        }
        tie(edge, flag) = boost::edge(indices[from.m_value], indices[to.m_value], *graph_merged);
        if (!flag) {
            tie(edge, flag) = boost::add_edge(indices[from.m_value], indices[to.m_value], 0.0f, *graph_merged);
            boost::get(vertex_name_merged, indices[from.m_value]) = indices[from.m_value];
            boost::get(vertex_name_merged, indices[to.m_value]) = indices[to.m_value];
        }
    }

    if (add_further_edges) {
        for (size_t i = 0; i < connected_clouds.size(); ++i) {
            for (size_t j = 0; j < i; ++j) {
                tie(edge, flag) = boost::edge(i, j, *graph_merged);
                if (flag) {
                    continue;
                }
                bool done = false;
                for (int ii = 0; ii < connected_clouds[i]->size(); ii += 20) {
                    for (int jj = 0; jj < connected_clouds[j]->size(); jj += 20) {
                        if ((connected_clouds[i]->points[ii].getVector3fMap() -
                             connected_clouds[j]->points[jj].getVector3fMap()).norm() < 0.2f) {
                            tie(edge, flag) = boost::add_edge(i, j, 0.0f, *graph_merged);
                            boost::get(vertex_name_merged, i) = i;
                            boost::get(vertex_name_merged, j) = j;
                            done = true;
                            break;
                        }
                    }
                    if (done) {
                        break;
                    }
                }
            }

        }
    }

    //vector<Graph*> graphs_out;
    //connected_components(graphs_out, *graph_merged);
    //visualize_segments(graphs_out, connected_clouds);

    return graph_merged;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * @brief supervoxel_segmentation::compute_convex_oversegmentation                                                     *
 * @param cloud_in - the point cloud to be segmented                                                                   *
 * @param visualize - determines if resultung segmentation is visualized                                               *
 * @return (supervoxel graph, supervoxel clouds, convex segment clouds, map supervoxel index -> convex segment index)  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
tuple<supervoxel_segmentation::Graph*, supervoxel_segmentation::Graph*,
      vector<supervoxel_segmentation::CloudT::Ptr>, vector<supervoxel_segmentation::CloudT::Ptr>, map<size_t, size_t> >
supervoxel_segmentation::compute_convex_oversegmentation(CloudT::Ptr& cloud_in, bool visualize)
{
    // why am I still doing this? should not be needed!
    CloudT::Ptr subsampled_cloud(new CloudT);
    subsample_cloud(subsampled_cloud, cloud_in);

    vector<CloudT::Ptr> voxel_segments;
    vector<CloudT::Ptr> rgb_segments;
    Graph* graph_in = create_supervoxel_graph(voxel_segments, rgb_segments, subsampled_cloud);
    Graph* graph_copy = new Graph;
    boost::copy_graph(*graph_in, *graph_copy);
    vector<Graph*> graphs_convex;
    recursive_split(graphs_convex, *graph_in);

    cout << "Segment size:" << endl;
    cout << voxel_segments.size() << endl;
    cout << rgb_segments.size() << endl;

    if (visualize) {
        visualize_segments(graphs_convex, rgb_segments);
    }
    vector<Graph*> graphs_out = color_model_split(graphs_convex, voxel_segments);
    if (visualize) {
        visualize_segments(graphs_out, rgb_segments);
    }

    // we need to port post_merge convex_segments to work in the same way

    //color_model_split(graphs_out);

    cout << "Graphs size: " << graphs_out.size() << endl;

    //vector<CloudT::Ptr> full_segments;
    /*
    vector<CloudT::Ptr> clouds_out;
    vector<CloudT::Ptr> supervoxel_segments;
    map<size_t, size_t> indices;
    create_full_segment_clouds(clouds_out, supervoxel_segments, indices, voxel_segments, cloud_in, graphs_out);
    */
    vector<CloudT::Ptr> connected_clouds;
    map<size_t, size_t> indices;
    tie(connected_clouds, indices) = merge_connected_clouds(rgb_segments, graphs_out);

    Graph* connected_graph = create_merged_graph(*graph_copy, graphs_out, indices, connected_clouds);

    //vector<CloudT::Ptr> clouds_out;
    //post_merge_convex_segments(clouds_out, indices, full_segments, *graph_copy); // seriously, this should operate on the graph

    for (Graph* g : graphs_out) {
        delete g;
    }

    /*if (visualize) {
        visualize_segments(full_segments, true);
    }
    if (visualize) {
        visualize_segments(clouds_out, true);
    }*/

    // segments need to be the full versions from e.g. create_full_segments_clouds
    return make_tuple(graph_copy, connected_graph, rgb_segments, connected_clouds, indices);
}

tuple<supervoxel_segmentation::Graph*, supervoxel_segmentation::Graph*,
      vector<supervoxel_segmentation::CloudT::Ptr>, vector<supervoxel_segmentation::CloudT::Ptr>, map<size_t, size_t> >
supervoxel_segmentation::compute_convex_oversegmentation(CloudT::Ptr& cloud_in, NormalCloudT::Ptr& normals_in, bool visualize)
{
    vector<CloudT::Ptr> voxel_segments;
    vector<CloudT::Ptr> rgb_segments;
    Graph* graph_in = create_supervoxel_graph(voxel_segments, rgb_segments, cloud_in, &normals_in);
    Graph* graph_copy = new Graph;
    boost::copy_graph(*graph_in, *graph_copy);
    vector<Graph*> graphs_convex;
    recursive_split(graphs_convex, *graph_in);

    cout << "Segment size:" << endl;
    cout << voxel_segments.size() << endl;
    cout << rgb_segments.size() << endl;

    if (visualize) {
        visualize_segments(graphs_convex, rgb_segments);
    }
    /*vector<Graph*> graphs_out = color_model_split(graphs_convex, voxel_segments);
    if (visualize) {
        visualize_segments(graphs_out, rgb_segments);
    }*/

    //cout << "Graphs size: " << graphs_out.size() << endl;

    vector<CloudT::Ptr> connected_clouds;
    map<size_t, size_t> indices;
    tie(connected_clouds, indices) = merge_connected_clouds(rgb_segments, graphs_convex);

    Graph* connected_graph = create_merged_graph(*graph_copy, graphs_convex, indices, connected_clouds);

    for (Graph* g : graphs_convex) {
        delete g;
    }

    // segments need to be the full versions from e.g. create_full_segments_clouds
    return make_tuple(graph_copy, connected_graph, rgb_segments, connected_clouds, indices);
}

void supervoxel_segmentation::save_graph(Graph& g, const string& filename) const
{
    serialized_graph<Graph> sg;
    sg.from_graph(g);
    ofstream out(filename, std::ios::binary);
    {
        cereal::BinaryOutputArchive archive_o(out);
        archive_o(sg);
    }
    out.close();
}

void supervoxel_segmentation::load_graph(Graph& g, const string& filename) const
{
    serialized_graph<Graph> sg;
    ifstream in(filename, std::ios::binary);
    {
        cereal::BinaryInputArchive archive_i(in);
        archive_i(sg);
    }
    in.close();
    sg.to_graph(g);
}
