//
//  PointPairFeatures.cpp
//  PointPairFeatures
//
//  Created by Adrian Haarbach on 02.07.14.
//  Copyright (c) 2014 Adrian Haarbach. All rights reserved.
//

#include "PointPairFeatures.h"

#include <iostream>     // std::cout, std::fixed
#include <iomanip>      // std::setprecision

void PointPairFeatures::printBucket(Bucket v){
    cout<<v.size()<< "::::";
    for(auto i : v){
        i.print();
    }
}

void PointPairFeatures::printMap(GlobalModelDescription m){
    for (auto kv : m) {
        cout << &kv.first << " : ";
        printBucket(kv.second);
        cout << endl;
    }
}


KeyBucketPairList PointPairFeatures::print10(GlobalModelDescription &mymap) {
    KeyBucketPairList myvec(mymap.begin(), mymap.end());
    assert(myvec.size() >= 10);
    //std::partial_sort(myvec.begin(), myvec.begin() + 10, myvec.end(),
    std::sort(myvec.begin(), myvec.end(),
                      [](const KeyBucketPair &lhs, const KeyBucketPair &rhs) {
                          return lhs.second.size() > rhs.second.size();
                      });
    
    for (int i = 0; i < 10; ++i) {
        cout<<"the 10 largest buckets are:"<<endl;
        cout<<"size="<<myvec[i].second.size()<<endl;//<<" key= "<<myvec[i].first;
        //printBucket(myvec[i].second);
        cout<<endl;
        //std::cout << i << ": " << myvec[i].first << "-> " << myvec[i].second << "\n";
    }
    
    return myvec;
}


GlobalModelDescription PointPairFeatures::buildGlobalModelDescription(MatrixXd m){
    
    RowVectorXd p1(6),p2(6);

    
    GlobalModelDescription map;
    
    Nm=m.rows();
    
    int numPPFs=0;
    
    for (int i=0; i<Nm; i++) {
        p1=m.row(i);
        for (int j=0; j<Nm; j++) {
            if(i==j) continue;
            p2=m.row(j);
            
            //p1 << 0,0,0,  1,0,0;
            //p2 << 1,1,0,  0,1,0;
            
            //std::cout << p1 << std::endl;
            //std::cout << p2 << std::endl;
            numPPFs++;
            
            PPF ppf=PPF::makePPF(p1,p2,i,j);
            
            map[ppf].push_back(ppf);
        }
    }
    
    vector<double> numb;
    
    for (auto it : map){
        double x=it.second.size();
        numb.push_back(x);
    }
    
    cout<<"PPF's discretisation values: ddist="<<ddist<<" dangle"<<dangle<<endl;
    cout<<"Built GlobalModelDescription from: "<<Nm<<" pts, yielding "<<numPPFs<< " PPF's hashed into "<<map.size()<<" Buckets: "<<endl;
    LoadingSaving::summary(numb);
    //LoadingSaving::saveVector("buckets.txt", numb);
    
    return map;
    
}

Matches PointPairFeatures::matchSceneAgainstModel(MatrixXd m, GlobalModelDescription model){

    Matches matches;

    RowVectorXd p1(6),p2(6);
    
    long Sm=m.rows(); //number of model sample points
    
    numberOfSceneRefPts=0.2*Sm;
    //numberOfSceneRefPts=1;

    for (int index=0; index<numberOfSceneRefPts; index++) {
        
        int i=rand() % Sm;
        i=index;//Testing
        p1=m.row(i);
        sceneIndexToI.push_back(i);
        
        for (int j=0; j<m.rows(); j++) {
            if(i==j) continue;
            p2=m.row(j);
                        
            PPF ppf = PPF::makePPF(p1,p2,i,j);
            ppf.index=index;
            
            auto it = model.find(ppf);
            
            if(it != model.end()){
                Bucket modelBucket = it->second;
                Match match = {ppf,modelBucket};
                matches.push_back(match);
            }else{
                //cout<<"no match index="<<index<<"  j="<<j<<endl;
            }
        }
    }
    
    vector<double> numb;
    
    for (auto it : matches){
        double x=it.modelPPFs.size();
        //cout<<it.scenePPF.i<<" "<<it.scenePPF.j<<" model bucket size="<<x<<endl;
        numb.push_back(x);
    }
    
    cout<<"Matched PPF's from "<<numberOfSceneRefPts<<" Scene Reference Points against "<<model.size()<<" Model buckets:"<<endl;
    LoadingSaving::summary(numb);
    //LoadingSaving::saveVector("buckets_matched.txt", numb);

    return matches;

}


vector<MatrixXi> PointPairFeatures::voting(Matches matches){
    vector<MatrixXi> accVec;
    for (int i=0; i<numberOfSceneRefPts; i++) {
        MatrixXi acc=MatrixXi::Zero(Nm,nangle);
        accVec.push_back(acc);
    }

    
    cout<<"Voting for ACC"<<numberOfSceneRefPts<<"*"<<accVec[0].rows()<<"*"<<accVec[0].cols()<<endl;
    
    
    for (auto it : matches){
        int sr=it.scenePPF.index;
        if(std::isnan(it.scenePPF.alpha)){
            cout<<sr<<" sr isnan"<<endl;
            continue;
        }
        //MatrixXi acc=accVec[sr];
        for (auto it1:it.modelPPFs){
            long mr=it1.i;
            if(std::isnan(it1.alpha)){
                cout<<mr<<" mr isnan"<<endl;
                continue;
            }
            double alpha=abs(abs(it1.alpha)-abs(it.scenePPF.alpha));
            
            int alphaDiscretised=alpha/dangle;
            /*long r=acc.rows();
            long c=acc.cols();
            if(mr<0 || mr>=r || alphaDiscretised>=c || alphaDiscretised <0){
                cout<<mr<<"*"<<alphaDiscretised<<endl;
            }*/
            accVec[sr](mr,alphaDiscretised)=accVec[sr](mr,alphaDiscretised)+1;
        }
        //cout<<acc<<endl;
    }
    return accVec;
}

Poses PointPairFeatures::computePoses(vector<MatrixXi> accVec, MatrixXd m, MatrixXd s){
    Poses vec;

    for (int index=0; index<numberOfSceneRefPts; index++) {
        MatrixXi acc=accVec[index];

        int sr=sceneIndexToI[index];
        int mr;
        int alphaD;
        int score=acc.maxCoeff(&mr, &alphaD); //TODO detect multiple peaks, but ask betram if this only happens if there are multiple object instances in the scene


        double alpha=alphaD*dangle;

        RowVector3d m1=m.block(mr, 0, 1, 3);
        RowVector3d s1=s.block(sr, 0, 1, 3);

        Translation3d Tgs(s1);
        AngleAxisd Rx(alpha, Vector3d::UnitX());

        Translation3d Tmg(-m1);

        Projective3d P(Tgs*Rx*Tmg);

        cout<<"Pose: "<<index<<" score:"<<score<<" alpha:"<<alpha<<endl;
        cout<<"computed:"<<endl;
        cout<<P.matrix()<<endl;

        vec.push_back(std::make_pair(P,score));
    }
    
    return vec;
}

//returns true if farthest neighbors in cluster fit within threshold
//http://en.wikipedia.org/wiki/Complete-linkage_clustering
bool PointPairFeatures::isClusterSimilar(Poses cluster1, Poses cluster2){
    for(auto pose2 : cluster2){
        bool isSimilar = std::all_of(cluster1.begin(), cluster1.end(), [&](Pose pose1){return isPoseSimilar(pose1.first, pose2.first);});
        if(!isSimilar) return false;
    }

    return true;
}

bool PointPairFeatures::isPoseSimilar(Projective3d P1, Projective3d P2){
    Vector3d    tra1 = P1.translation();
    Quaterniond rot1(P1.rotation());

    Vector3d    tra2 = P2.translation();
    Quaterniond rot2(P2.rotation());


    //Translation
    double diff_tra=(tra1-tra2).norm();
    double model_diameter = 0.15; //cm
    double thresh_tra = 0.05 * model_diameter; //double thresh_tra=0.02; //2cm



    //Rotation
    double d = rot1.dot(rot2);
    double diff_rot= 1 - d*d; //http://www.ogre3d.org/forums/viewtopic.php?f=10&t=79923

    double thresh_rot_degrees = 10;
    //http://math.stackexchange.com/questions/90081/quaternion-distance
    double thresh_rot=0.25; //0same, 1 180deg ////M_PI/10.0; //180/15 = 12
    double diff_rot_bertram = acos((rot1.inverse() * rot2).norm()); //bertram

    double diff_rot_degrees = degrees(acos(2*d - 1));

    //cout<<"diff_rot_0to1nor\t="<<diff_rot<<endl;
    //cout<<"diff_rot_bertram\t="<<diff_rot_bertram<<endl;

    cout<<std::fixed<<std::setprecision(3);

    cout<<"rot="<<diff_rot_degrees<<"<="<<thresh_rot_degrees<<" && tra="<<diff_tra<<"<= "<<thresh_tra<<" ?: ";

    if(diff_rot_degrees <= thresh_rot_degrees && diff_tra <= thresh_tra){
        cout<<"yes"<<endl;
        return true;
    }
    cout<<"no"<<endl;
    return false;
}

void PointPairFeatures::printPoses(Poses vec){
    for(auto pose : vec){
        cout<<"score="<<pose.second<<"mat=\n"<<pose.first.matrix()<<endl;
    }
    cout<<endl;
}

Pose PointPairFeatures::clusterPoses (Poses vec){
    //cout<<"clusterPoses"<<endl;
    //printPoses(vec);
    std::sort(vec.begin(), vec.end(), [](const Pose & a, const Pose & b) -> bool{ return a.second > b.second; });
    //cout<<"sorted"<<endl;
    //printPoses(vec);


   vector< Poses > clusters;
    
    for(auto pose : vec){
        Poses cluster;
        cluster.push_back(pose); //initially, each cluster contains just one pose;
        clusters.push_back(cluster);
    }

    int i=0;
    int n=clusters.size();

    for(int i=0; i<n; n=clusters.size(),i++){
        for(int j=0; j<n; n=clusters.size(),j++){
            if(i==j) continue;
            cout<<"Cluster1 "<<i<<"\\"<<n-1<<endl;
            Poses cluster1=clusters[i];
            cout<<"Cluster2 "<<j<<"\\"<<n-1<<endl;
            Poses cluster2=clusters[j];
            cout<<"size before merge:"<<cluster1.size()<<","<<cluster2.size()<<endl;

            if(isClusterSimilar(cluster1,cluster2)){
                cluster1.insert(cluster1.end(),cluster2.begin(),cluster2.end());
                clusters.erase(clusters.begin() + j);
            }
            cout<<"size after merge:"<<cluster1.size()<<","<<cluster2.size()<<endl;
            clusters[i]=cluster1;
        }
    }

    cout<<"Produced "<<clusters.size()<<" clusters with each #poses:"<<endl;
    for(auto cluster : clusters){
        cout<<cluster.size()<<endl;
        printPoses(cluster);
    }

    
    return clusters[0][0];
}

//Poses reduceCluster(vector<Poses> clusters){

//}
