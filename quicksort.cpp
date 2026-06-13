#include <cstdint>
#include <iostream>

using namespace std;

template <typename T>

void quick_sort(T* start, T* end)
{
    T temp = 0,*i = start - 1, *j = start;

    if(start == end || start>end)
    {
        return;
    }
    if(end - start == 1 )
    {
        if(*end < *start)
        {
            temp = *end;
            *end = *start;
            *start = temp;
        }
        
        return;
    }

    while(j < end)
    {
        if(*j <= *end)
        {
            i++;
            
            temp = *i;
            *i = *j;
            *j = temp;
        }
        j++;
    }
    i++;

    temp = *i;
    *i = *end;
    *end = temp;
    
    quick_sort(start, i-1);
    quick_sort(i+1,end);
    return;
}
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