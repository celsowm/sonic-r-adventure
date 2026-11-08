/*
 * Continuous translated VERTICAL capsule vs static roof TRIANGLE.
 * A capsule is the Minkowski sum of a finite vertical line segment and
 * a sphere. Contact is equivalent to distance(segment,triangle)<=radius.
 *
 * For each t we compute the exact closest-feature family in integer Q6:
 *  - either cap-center projected inside the triangle face,
 *  - intersection of the capsule's axis with the triangle,
 *  - or closest points between the axis and each triangle edge.
 * For vertical axes, nearest edge contacts reduce to a finite set of
 * 1D quadratic candidates; no nested point-sampling or allocations.
 *
 * Squared distance between two translated convex bodies is convex in t.
 * A bounded ternary bracket (plateau-aware) plus integer first-hit bisection
 * finds the earliest contact at Q12 tick resolution, including shafts,
 * top/bottom caps, triangle edges and vertices. All world coordinates and
 * arithmetic use integer fixed point, including the contact normal.
 *
 * Not a solver for arbitrary moving/rotating geometry or mesh concavities.
 * Roofs are individual opt-in, moderate-slope BIN triangles.
 */
#include "adventure_capsule.h"
#include <stdint.h>

#define CAP_COORD_DIV 64                /* convert Q20.12 -> Q6 */
#define CAP_CONTACT_SKIN_Q6 1           /* one 1/64-world-unit precision unit */
#define CAP_MAX_EDGE_WORLD 8192
#define CAP_MAX_RADIUS (32*ADV_FP_ONE)
#define CAP_MAX_HEIGHT (256*ADV_FP_ONE)

typedef struct CapVec {
    int64_t x, y, z;
} CapVec;
typedef struct CapClosest {
    uint64_t squared;
    CapVec axis, triangle;
} CapClosest;
typedef struct CapFixture {
    CapVec a,b,c;
    int64_t nx,ny,nz;
    uint32_t normal_length;
} CapFixture;
static uint64_t abs_i64(int64_t n)
{
    return n<0 ? (uint64_t)(-(n+1))+1u : (uint64_t)n;
}
static uint64_t squared(int64_t n)
{
    return (uint64_t)(n*n);
}
static uint64_t distance2(CapVec a, CapVec b)
{
    return squared(a.x-b.x)+squared(a.y-b.y)+squared(a.z-b.z);
}
static int64_t clamp(int64_t v,int64_t low,int64_t high)
{
    return v<low?low:(v>high?high:v);
}
static CapVec fixed_to_q6(AdventureVec3 v)
{
    CapVec out={v.x/CAP_COORD_DIV,v.y/CAP_COORD_DIV,
                v.z/CAP_COORD_DIV};
    return out;
}
static CapVec lerp_q6(CapVec a,CapVec b,uint32_t time)
{
    CapVec p={
        a.x+(b.x-a.x)*(int64_t)time/ADV_FP_ONE,
        a.y+(b.y-a.y)*(int64_t)time/ADV_FP_ONE,
        a.z+(b.z-a.z)*(int64_t)time/ADV_FP_ONE
    };
    return p;
}
static CapVec position_q6(AdventureVec3 from,AdventureVec3 to,uint32_t t)
{
    AdventureVec3 p={
        AdventureMath_ClampI32((int64_t)from.x+
                             ((int64_t)to.x-from.x)*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)from.y+
                             ((int64_t)to.y-from.y)*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)from.z+
                             ((int64_t)to.z-from.z)*t/ADV_FP_ONE)
    };
    return fixed_to_q6(p);
}
static bool make_fixture(const AdventureRoofTriangle *t,CapFixture *f)
{
    if (!t || !f) return false;
    int64_t ux=((int64_t)t->b.x-t->a.x)/ADV_FP_ONE;
    int64_t uy=((int64_t)t->b.y-t->a.y)/ADV_FP_ONE;
    int64_t uz=((int64_t)t->b.z-t->a.z)/ADV_FP_ONE;
    int64_t vx=((int64_t)t->c.x-t->a.x)/ADV_FP_ONE;
    int64_t vy=((int64_t)t->c.y-t->a.y)/ADV_FP_ONE;
    int64_t vz=((int64_t)t->c.z-t->a.z)/ADV_FP_ONE;
    if (abs_i64(ux)>CAP_MAX_EDGE_WORLD ||
        abs_i64(uy)>CAP_MAX_EDGE_WORLD ||
        abs_i64(uz)>CAP_MAX_EDGE_WORLD ||
        abs_i64(vx)>CAP_MAX_EDGE_WORLD ||
        abs_i64(vy)>CAP_MAX_EDGE_WORLD ||
        abs_i64(vz)>CAP_MAX_EDGE_WORLD) return false;
    f->nx=uy*vz-uz*vy;
    f->ny=uz*vx-ux*vz;
    f->nz=ux*vy-uy*vx;
    if (!f->ny) return false;
    if (f->ny<0) {
        f->nx=-f->nx; f->ny=-f->ny; f->nz=-f->nz;
    }
    if (abs_i64(f->nx)>(uint64_t)f->ny ||
        abs_i64(f->nz)>(uint64_t)f->ny) return false;
    f->normal_length=AdventureMath_IsqrtU64(
        squared(f->nx)+squared(f->ny)+squared(f->nz));
    if (!f->normal_length) return false;
    f->a=fixed_to_q6(t->a);
    f->b=fixed_to_q6(t->b);
    f->c=fixed_to_q6(t->c);
    return true;
}
static void consider(CapClosest *out,CapVec axis,CapVec triangle)
{
    uint64_t d=distance2(axis,triangle);
    if(d<out->squared) {
        out->squared=d;
        out->axis=axis;
        out->triangle=triangle;
    }
}
static int64_t cross2(int64_t ux,int64_t uz,int64_t vx,int64_t vz)
{
    return ux*vz-uz*vx;
}
static bool footprint(CapVec p,const CapFixture *f)
{
    /* Moderate roofs have a nondegenerate X/Z projection. */
    int64_t area=cross2(f->b.x-f->a.x,f->b.z-f->a.z,
                        f->c.x-f->a.x,f->c.z-f->a.z);
    if(!area) return false;
    int64_t e0=cross2(f->b.x-f->a.x,f->b.z-f->a.z,
                      p.x-f->a.x,p.z-f->a.z);
    int64_t e1=cross2(f->c.x-f->b.x,f->c.z-f->b.z,
                      p.x-f->b.x,p.z-f->b.z);
    int64_t e2=cross2(f->a.x-f->c.x,f->a.z-f->c.z,
                      p.x-f->c.x,p.z-f->c.z);
    /* Q6 tolerance of one unit absorbs only precision rounding. */
    int64_t eps=(int64_t)(
      abs_i64(f->b.x-f->a.x)+abs_i64(f->b.z-f->a.z)+
      abs_i64(f->c.x-f->b.x)+abs_i64(f->c.z-f->b.z)+
      abs_i64(f->a.x-f->c.x)+abs_i64(f->a.z-f->c.z)+1u);
    return area>0?(e0>=-eps && e1>=-eps && e2>=-eps):
                   (e0<=eps && e1<=eps && e2<=eps);
}
static void point_face(CapClosest *closest,CapVec point,const CapFixture *f)
{
    int64_t plane_num=f->nx*(point.x-f->a.x)+
                      f->ny*(point.y-f->a.y)+
                      f->nz*(point.z-f->a.z);
    int64_t distance=plane_num/f->normal_length;
    CapVec projected={
        point.x-distance*f->nx/f->normal_length,
        point.y-distance*f->ny/f->normal_length,
        point.z-distance*f->nz/f->normal_length
    };
    if (footprint(projected,f))
        consider(closest,point,projected);
}
static int64_t fraction_clamped(int64_t numerator,int64_t denominator)
{
    if(denominator<=0 || numerator<=0) return 0;
    if(numerator>=denominator) return ADV_FP_ONE;
    return numerator*ADV_FP_ONE/denominator;
}
static void edge_candidate(CapClosest *best,CapVec foot,CapVec head,
                           CapVec a,CapVec b,int64_t t)
{
    CapVec edge=lerp_q6(a,b,(uint32_t)clamp(t,0,ADV_FP_ONE));
    CapVec axis={foot.x,clamp(edge.y,foot.y,head.y),foot.z};
    consider(best,axis,edge);
}
static void axis_edge(CapClosest *best,CapVec foot,CapVec head,
                      CapVec a,CapVec b)
{
    CapVec d={b.x-a.x,b.y-a.y,b.z-a.z};
    int64_t len=d.x*d.x+d.y*d.y+d.z*d.z;
    int64_t horiz=d.x*d.x+d.z*d.z;
    CapVec q[2]={foot,head};
    edge_candidate(best,foot,head,a,b,0);
    edge_candidate(best,foot,head,a,b,ADV_FP_ONE);
    /* Candidate minima on the regions where the closest axis point is
     * at its lower cap, its upper cap, or somewhere along the shaft. */
    for(int i=0;i<2;i++) {
        int64_t dot=(q[i].x-a.x)*d.x+(q[i].y-a.y)*d.y+
                    (q[i].z-a.z)*d.z;
        edge_candidate(best,foot,head,a,b,fraction_clamped(dot,len));
    }
    edge_candidate(best,foot,head,a,b,fraction_clamped(
        (foot.x-a.x)*d.x+(foot.z-a.z)*d.z,horiz));
    if(d.y) {
        int64_t t0=(foot.y-a.y)*ADV_FP_ONE/d.y;
        int64_t t1=(head.y-a.y)*ADV_FP_ONE/d.y;
        edge_candidate(best,foot,head,a,b,t0);
        edge_candidate(best,foot,head,a,b,t1);
    }
}
static CapClosest closest_at(AdventureVec3 from,AdventureVec3 to,
                             int32_t height,int32_t radius,
                             const CapFixture *f,uint32_t t)
{
    CapVec p=position_q6(from,to,t);
    int64_t radius_q6=(int64_t)radius/CAP_COORD_DIV;
    int64_t top_q6=(int64_t)height/CAP_COORD_DIV;
    CapVec bottom={p.x,p.y+radius_q6,p.z};
    CapVec head={p.x,p.y+top_q6-radius_q6,p.z};
    CapClosest best={UINT64_MAX,{0,0,0},{0,0,0}};

    /* Closest points from both spherical cap centers to the triangle face. */
    point_face(&best,bottom,f);
    point_face(&best,head,f);

    /* If the axis itself intersects the face, distance is exactly zero.
     * This is the case which single-point head collision used to miss. */
    int64_t db=f->nx*(bottom.x-f->a.x)+
               f->ny*(bottom.y-f->a.y)+
               f->nz*(bottom.z-f->a.z);
    int64_t dh=f->nx*(head.x-f->a.x)+
               f->ny*(head.y-f->a.y)+
               f->nz*(head.z-f->a.z);
    if(db<=0 && dh>=0) {
        CapVec at={p.x,bottom.y-db/f->ny,p.z};
        if (footprint(at,f)) consider(&best,at,at);
    }
    /* Complete the closest-feature enumeration with all finite edges;
     * endpoint clamping includes triangle vertices automatically. */
    axis_edge(&best,bottom,head,f->a,f->b);
    axis_edge(&best,bottom,head,f->b,f->c);
    axis_edge(&best,bottom,head,f->c,f->a);
    return best;
}
static bool broadphase(AdventureVec3 from,AdventureVec3 to,
                       int32_t height,int32_t radius,const CapFixture *f)
{
    CapVec a=f->a,b=f->b,c=f->c;
    int64_t mnx=a.x,mxx=a.x,mny=a.y,mxy=a.y,mnz=a.z,mxz=a.z;
    const CapVec tri[2]={b,c};
    for(int i=0;i<2;i++){
        CapVec p=tri[i];
        if(p.x<mnx) mnx=p.x;
        if(p.x>mxx) mxx=p.x;
        if(p.y<mny) mny=p.y;
        if(p.y>mxy) mxy=p.y;
        if(p.z<mnz) mnz=p.z;
        if(p.z>mxz) mxz=p.z;
    }
    CapVec start=fixed_to_q6(from),end=fixed_to_q6(to);
    int64_t r=((int64_t)radius+CAP_COORD_DIV-1)/CAP_COORD_DIV;
    int64_t h=((int64_t)height+CAP_COORD_DIV-1)/CAP_COORD_DIV;
    int64_t lx=start.x<end.x?start.x:end.x;
    int64_t hx=start.x>end.x?start.x:end.x;
    int64_t lz=start.z<end.z?start.z:end.z;
    int64_t hz=start.z>end.z?start.z:end.z;
    int64_t ly=start.y<end.y?start.y:end.y;
    int64_t hy=(start.y>end.y?start.y:end.y)+h;
    return !(hx+r<mnx || lx-r>mxx ||
             hy<mny || ly>mxy || hz+r<mnz || lz-r>mxz);
}
static void normal_at(AdventureCapsuleHit *hit,CapClosest closest,
                      AdventureVec3 from,AdventureVec3 to,
                      const CapFixture *f)
{
    int64_t x=closest.axis.x-closest.triangle.x;
    int64_t y=closest.axis.y-closest.triangle.y;
    int64_t z=closest.axis.z-closest.triangle.z;
    uint32_t len=AdventureMath_IsqrtU64(squared(x)+squared(y)+squared(z));
    if(len) {
        hit->normal.x=AdventureMath_ClampI32(x*ADV_FP_ONE/len);
        hit->normal.y=AdventureMath_ClampI32(y*ADV_FP_ONE/len);
        hit->normal.z=AdventureMath_ClampI32(z*ADV_FP_ONE/len);
        return;
    }
    /* Axis/face overlap has no unique witness normal. Use the roof plane
     * oriented AGAINST movement to keep the response deterministic. */
    int64_t approach=f->nx*((int64_t)to.x-from.x)+
                     f->ny*((int64_t)to.y-from.y)+
                     f->nz*((int64_t)to.z-from.z);
    int32_t sign=approach>0?-1:1;
    hit->normal.x=AdventureMath_ClampI32(
        sign*f->nx*ADV_FP_ONE/f->normal_length);
    hit->normal.y=AdventureMath_ClampI32(
        sign*f->ny*ADV_FP_ONE/f->normal_length);
    hit->normal.z=AdventureMath_ClampI32(
        sign*f->nz*ADV_FP_ONE/f->normal_length);
}
bool AdventureCapsule_SweepTriangle(AdventureVec3 from,AdventureVec3 to,
                                   int32_t height,int32_t radius,
                                   const AdventureRoofTriangle *triangle,
                                   AdventureCapsuleHit *out)
{
    CapFixture f;
    if (!out || height<=0 || radius<=0 ||
        radius>CAP_MAX_RADIUS || height>CAP_MAX_HEIGHT ||
        (int64_t)radius*2>height || !make_fixture(triangle,&f) ||
        !broadphase(from,to,height,radius,&f)) return false;

    /* A swept capsule and a static triangle are convex sets; squared
     * separation distance is convex in t. Quantization can create flat
     * plateaus, so preserve the middle interval when ternary probes tie. */
    uint32_t lo=0,hi=ADV_FP_ONE;
    while (hi-lo>24) {
        uint32_t third=(hi-lo)/3;
        uint32_t left=lo+third,right=hi-third;
        uint64_t dl=closest_at(from,to,height,radius,&f,left).squared;
        uint64_t dr=closest_at(from,to,height,radius,&f,right).squared;
        if(dl<dr) hi=right;
        else if(dl>dr) lo=left;
        else {lo=left;hi=right;}
    }
    uint32_t min_t=lo;
    CapClosest minimum=closest_at(from,to,height,radius,&f,lo);
    for(uint32_t t=lo+1;t<=hi;t++){
        CapClosest next=closest_at(from,to,height,radius,&f,t);
        if(next.squared<minimum.squared) {
            minimum=next;min_t=t;
        }
    }
    uint64_t r_q6=((uint64_t)radius+CAP_COORD_DIV-1u)/CAP_COORD_DIV;
    uint64_t limit=r_q6*r_q6;
    if(minimum.squared>limit) return false;

    CapClosest start=closest_at(from,to,height,radius,&f,0);
    uint32_t first=0;
    if(start.squared<=limit) {
        /* Don't glue the body to geometry it is leaving. */
        if(minimum.squared>=start.squared) return false;
    }else{
        uint32_t left=0,right=min_t;
        while(left<right) {
            uint32_t mid=(left+right)/2u;
            if(closest_at(from,to,height,radius,&f,mid).squared<=limit)
                right=mid;
            else
                left=mid+1;
        }
        first=left;
    }
    CapClosest contact=closest_at(from,to,height,radius,&f,first);
    out->fraction=(uint16_t)first;
    out->triangle_id=triangle->id;
    out->source=ADV_CONTACT_BIN_TRIANGLE;
    out->has_resolved_y=false;
    out->resolved_y=0;
    normal_at(out,contact,from,to,&f);
    return true;
}
bool AdventureCapsule_SweepMesh(void *context,AdventureVec3 from,
                               AdventureVec3 to,int32_t height,
                               int32_t radius,AdventureCapsuleHit *out)
{
    const AdventureRoofMesh *mesh=(const AdventureRoofMesh *)context;
    if(!mesh || !out) return false;
    bool found=false;
    AdventureCapsuleHit best={0};
    for(uint16_t i=0;i<mesh->count;i++) {
        AdventureCapsuleHit hit;
        if(AdventureCapsule_SweepTriangle(from,to,height,radius,
                                          &mesh->triangles[i],&hit) &&
           (!found || hit.fraction<best.fraction)) {
            best=hit;found=true;
        }
    }
    if(found) *out=best;
    return found;
}
