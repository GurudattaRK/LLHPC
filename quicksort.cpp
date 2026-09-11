#include <iostream>
// #include <cstdint>

template<typename T>
void quick_sort(T* start, T* end)
{
    T* i = start - 1;
    T* j = start ;
    T* pivot = end;
    if(start>=end)
    {
        std::cout<<"Early exit: start matches or exceedes end\n";
        return ;
    }
    else if (end - start == 1)
    {
        std::swap(*start, *end);
        std::cout<<"Early exit: start is next to end, swapped\n";
        return ;
    }
    else
    {
        while(j<pivot) //keep moving untilwe reach pivot's location
        {// i is ALWAYS pointing to position JUST BEHIND position of element bigger than pivot in last iteration 
            //or pointing to pre-start if this is the first iteration. i is waiting ...
            if(*j < *pivot) // ... as soon as j finds a smaller element it can now use this smaller element to swap with
            {// bigger element that was right infront of i's position
                i++; // we move i forward so it points to last BIG element..
                std::swap(*i , *j); //..we swap the newly found small element with last found BIG elements positions.
                
            }// now i is pointing to small element  and j is pointing to BIG element, which for now is fine 
            // because BIG element might be on right side of pivot's final position and i's small element is anyway of left side.
            
            j++; // we move forward with j
            
            //A.later if we find a bigger element using j then we keep going(ignore it for now) & it waits and keep looking 
            // until it finds a smaller element so that we can use that smaller element to swap with bigger element we found earlier pointed by i.
            // B. later if we find a smaller element using j then we do the swap, which basically swaps smaller elements between i and j
            // and swap i gets moved forward, this keeps happening until is right behind a BIGGER element.
            // 
            // The swap and increment 'i' process keeps happening regardless of if 'i' is pointing to smaller or bigger element.
        }
        i++;// after j has reached end(pivot) location, time to finish this stack call
        std::swap(*i,*pivot);//in the end 'i' ends up pointing at the location just behind where pivot would end up in final result
        // so we increment i to point to that correct pivot location and swap values of i and pivot so that pivot value moves into
        // correct pivot position(which was pointed by i before swap)
    }

    quick_sort(start,i-1);
    quick_sort(i+1,end);
    return;
}
using namespace std;
int main()
{
    int array[]= {4,0,9,2,7,3,6,1,5,8};
    cout<<"Pre-sort"<<endl;

    for(int i=0;i<10;i++)
    {
        cout<<array[i]<<",";
    }

    cout<<endl;
    cout<<endl;
    quick_sort(array, array +9);
    cout<<"Post-sort"<<endl;

    for(int i=0;i<10;i++)
    {
        cout<<array[i]<<",";
    }
    cout<<endl;
    return 0;
}