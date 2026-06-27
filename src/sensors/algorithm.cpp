#include "algorithm.h"
#include <math.h>

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *n_npks, int32_t  *pn_x, int32_t n_size, int32_t n_min_height)
{
    int32_t i = 1, n_width;
    *n_npks = 0;
    while (i < n_size-1){
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i-1]){
            n_width = 1;
            while (i+n_width < n_size && pn_x[i] == pn_x[i+n_width]) n_width++;
            if (pn_x[i] > pn_x[i+n_width] && (*n_npks) < 15 ){
                pn_locs[(*n_npks)++] = i;
                i += n_width+1;
            } else i += n_width;
        } else i++;
    }
}

void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
{
    int32_t i, j, n_old_npks, n_dist;
    
    // Sort peaks
    for (i = 1; i < *pn_npks; i++) {
        int32_t temp_loc = pn_locs[i];
        for (j = i; j > 0 && pn_locs[j - 1] > temp_loc; j--) {
            pn_locs[j] = pn_locs[j - 1];
        }
        pn_locs[j] = temp_loc;
    }
    
    // Remove close peaks
    for (i = -1; i < *pn_npks; i++) {
        n_old_npks = *pn_npks;
        *pn_npks = i + 1;
        for (j = i + 1; j < n_old_npks; j++) {
            n_dist =  pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
            if (n_dist > n_min_distance || (n_dist == -1)) {
                pn_locs[(*pn_npks)++] = pn_locs[j];
            }
        }
    }
}

void maxim_find_peaks(int32_t *pn_locs, int32_t *n_npks, int32_t  *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
{
    maxim_peaks_above_min_height(pn_locs, n_npks, pn_x, n_size, n_min_height);
    maxim_remove_close_peaks(pn_locs, n_npks, pn_x, n_min_distance);
    if (*n_npks > n_max_num) *n_npks = n_max_num;
}

const uint16_t auw_hamm[31]={ 41,    276,    512,    276,     41 };

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
    uint32_t un_ir_mean;
    int32_t k, n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t n_th1, n_npks;
    int32_t an_ir_valley_locs[15] ;
    int32_t n_peak_interval_sum;
    
    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc;
    int32_t n_y_dc_max, n_x_dc_max;
    int32_t n_y_dc_max_idx, n_x_dc_max_idx;
    int32_t an_ratio[5], n_ratio_average;
    int32_t n_nume, n_denom ;

    // calculates DC mean and subtract DC from ir
    un_ir_mean =0;
    for (k=0 ; k<n_ir_buffer_length ; k++ ) un_ir_mean += pun_ir_buffer[k] ;
    un_ir_mean =un_ir_mean/n_ir_buffer_length ;
    
    int32_t an_x[n_ir_buffer_length];
    for (k=0 ; k<n_ir_buffer_length ; k++ )  an_x[k] =  -1*(pun_ir_buffer[k] - un_ir_mean) ;
    
    n_th1 = 0;
    for (k=0 ; k<n_ir_buffer_length ; k++ ) n_th1 += an_x[k];
    n_th1 = n_th1 / n_ir_buffer_length;
    if (n_th1 < 30) n_th1 = 30; // min allowed
    
    maxim_find_peaks( an_ir_valley_locs, &n_npks, an_x, n_ir_buffer_length, n_th1, 4, 15 );
    
    n_peak_interval_sum =0;
    if (n_npks >= 2){
        for (k=1; k<n_npks; k++) n_peak_interval_sum += (an_ir_valley_locs[k] -an_ir_valley_locs[k -1] ) ;
        n_peak_interval_sum =n_peak_interval_sum/(n_npks-1);
        *pn_heart_rate =(int32_t)( (6000 / n_peak_interval_sum) );
        *pch_hr_valid  = 1;
    }
    else  { 
        *pn_heart_rate = -999; 
        *pch_hr_valid  = 0;
    }
    
    n_exact_ir_valley_locs_count = n_npks;
    n_ratio_average = 0;
    n_i_ratio_count = 0;
    
    for(k=0; k< 5; k++) an_ratio[k]=0;
    for (k=0; k< n_exact_ir_valley_locs_count; k++){
        if (an_ir_valley_locs[k] > 0){
            n_x_dc_max = -16777216 ;
            n_x_dc_max_idx = -1;
            n_y_dc_max = -16777216 ;
            n_y_dc_max_idx = -1;
            
            for (i=0; i<15; i++) {
                s = an_ir_valley_locs[k] - i;
                if (s>=0) {
                    if (pun_ir_buffer[s] > n_x_dc_max) { n_x_dc_max = pun_ir_buffer[s]; n_x_dc_max_idx = s; }
                    if (pun_red_buffer[s] > n_y_dc_max) { n_y_dc_max = pun_red_buffer[s]; n_y_dc_max_idx = s; }
                }
            }
            n_y_ac = (pun_red_buffer[an_ir_valley_locs[k]] - n_y_dc_max );
            n_x_ac = (pun_ir_buffer[an_ir_valley_locs[k]] - n_x_dc_max );
            if ((n_y_ac != 0) && (n_x_ac != 0)) {
                n_nume = (n_y_ac * n_x_dc_max) >> 7 ;
                n_denom = (n_x_ac * n_y_dc_max) >> 7;
                if (n_denom > 0  && n_i_ratio_count < 5 &&  n_nume != 0) {
                    an_ratio[n_i_ratio_count] = (n_nume * 100) / n_denom ;
                    n_i_ratio_count++;
                }
            }
        }
    }
    
    if (n_i_ratio_count > 0) {
        for (k=0; k<n_i_ratio_count; k++) n_ratio_average += an_ratio[k];
        n_ratio_average /= n_i_ratio_count;
        n_spo2_calc = 104 - 17 * n_ratio_average / 100;
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }
}
