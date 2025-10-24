# include <stdio.h>
# include <stdlib.h>
#include <pthread.h>

#include "../common/core.h"

#define TABLE_SIZE 100000000
#define LOCK_COUNT 4096


static customer_info *hash_table[TABLE_SIZE];
//bucket level locks, same as Java's ConcurrentHashMap
static pthread_mutex_t bucket_locks[LOCK_COUNT];


void init_locks() {
    for (int i = 0; i < LOCK_COUNT; i++) {
        pthread_mutex_init(&bucket_locks[i], NULL);
    }
}

int get_lock_index(int bucket_index) {
    return bucket_index % (LOCK_COUNT - 1);
} 

unsigned int hash(unsigned int key){
    return key % TABLE_SIZE;
}

/**
 * This method expects caller method to handle concurrency, ie
 * lock and unlock pthread_mutex from the calling method itself.
 * 
 * This method assumes itself to be thread safe while shifting the 
 * responsibility of concurrency handling to the caller.
 */
customer_info* look_up_cust_id(int custId) {
    
    unsigned int index = hash(custId);
    customer_info *hashmap_value = hash_table[index];
    if (hashmap_value != NULL) {
       if (hashmap_value->custId == custId) {
            return hashmap_value;
       }
       else {
            customer_info *headPtr = hashmap_value;
            customer_info *nextPtr = hashmap_value->next;
            while (nextPtr != NULL) {
                if (nextPtr->custId == custId) {
                    return nextPtr;
                }
                nextPtr = nextPtr->next;
            }
       }
    }  
    return NULL;
}

int getEmptyIndexInTransactionArrayOfCustomer( customer_info* customer_info ) {
    for (int i = 0 ; i < MAX_TRXNS_PER_CUSTOMER ; i++) {
        if (customer_info->latest_trxns[i].trxnDate == -1) {
            return i;
        }
    }
    /* All indexes are filled, we need to shift the array elements to the left
    while shifting the elements to the left, the oldest array element will get 
    discarded, and new empty spot will appear at the end of the array.
    */
   //shift array to left by 1 element alone.
   for (int i = 0 ; i < MAX_TRXNS_PER_CUSTOMER - 1 ; i++) {
        customer_info->latest_trxns[i]=customer_info->latest_trxns[i+1];
    }
    transaction_details dummy = {-1, 0.0, 0.0, 0.0};
    customer_info->latest_trxns[MAX_TRXNS_PER_CUSTOMER - 1] = dummy;
    return MAX_TRXNS_PER_CUSTOMER - 1;
}

void ffi_init_hash_table() {
    for (int i = 0 ; i < TABLE_SIZE ; i++) {
        hash_table[i] = NULL;
    }
    init_locks();
}

void ffi_insert_to_hash_table(int custId, char firstName[MAX_NAME_CHARS], char lastName[MAX_NAME_CHARS],
char phone[MAX_PHONE_CHARS], char email[MAX_EMAIL_CHARS], double balance) {
    
    if (custId == INVALID_INT) {
        return;
    }

    int index = hash(custId); 
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
   
    if (hash_table[index] == NULL) {
         customer_info* mapEntry = transformBasicInputToCustomerInfo(custId, firstName, lastName, phone, email, balance);
         hash_table[index] = mapEntry;   
         pthread_mutex_unlock(&bucket_locks[lock_index]);
         return;
    }

    customer_info *headPtr = hash_table[index];
    if (headPtr->custId  == custId) {
        //update cache value for the custId.
        strncpy(headPtr->firstName, firstName, MAX_NAME_CHARS);
        strncpy(headPtr->lastName, lastName, MAX_NAME_CHARS);
        strncpy(headPtr->phone, phone, MAX_PHONE_CHARS);
        strncpy(headPtr->email, email, MAX_EMAIL_CHARS);
        headPtr->balance = balance;
        pthread_mutex_unlock(&bucket_locks[lock_index]);
        return;
    }
    else {
        customer_info *prevPtr = headPtr;
        customer_info *nextPtr = headPtr->next;
        while (nextPtr != NULL) {
            if (nextPtr->custId == custId) {
                strncpy(nextPtr->firstName, firstName, MAX_NAME_CHARS);
                strncpy(nextPtr->lastName, lastName, MAX_NAME_CHARS);
                strncpy(nextPtr->phone, phone, MAX_PHONE_CHARS);
                strncpy(nextPtr->email, email, MAX_EMAIL_CHARS);
                nextPtr->balance = balance;
                pthread_mutex_unlock(&bucket_locks[lock_index]);
                return;
        }
        prevPtr = nextPtr;
        nextPtr = nextPtr->next;
    }
    //if control is here that means, we need to add a brand new node at the end of the linked list
    customer_info* mapEntry = transformBasicInputToCustomerInfo(custId, firstName, lastName, phone, email, balance);
    mapEntry->next = NULL;
    prevPtr->next = mapEntry;
    pthread_mutex_unlock(&bucket_locks[lock_index]);
    return;
    }   
}

void ffi_update_basic_customer_info(int custId, char phone[MAX_PHONE_CHARS], char email[MAX_EMAIL_CHARS]) {
    
    if (custId == INVALID_INT) {
        return;
    }

    int index = hash(custId); 
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);

    customer_info* customer_info = look_up_cust_id(custId);
    if (customer_info == NULL) {
         //custInfo is not found in cache. Nothing to do here. 
         pthread_mutex_unlock(&bucket_locks[lock_index]);
         return;
    }

    strncpy(customer_info->phone, phone, MAX_PHONE_CHARS);
    strncpy(customer_info->email, email, MAX_EMAIL_CHARS);
    pthread_mutex_unlock(&bucket_locks[lock_index]);    
}

/**
 * Why is the returnJsonBasicInfo being passed from Java ? 
 * Observe the methods that update entries into your hash_table. 
 * 
 * To ensure data is not corrupted during reading/writing by concurrent running Java threads, 
 * we are using pthread_mutex_init lock and unlock mechanism. This ensures that writing and reading
 * for same custId is mutually exclusive.
 * 
 * During lookup (read operation), we lock and unlock after finding the value for the looked up key (custId).
 * After unlock by read, a write operation may free the structure for that custId and refresh (again malloc) the  
 * hadshmap entry for that custId. This means, the returned value to Java may get freed by the time Java is reading it.
 * This will end up in Java getting corrupted value for reads. 
 * 
 * Now, if we decide to not free the memory during key overide during write, then the whole implementation logic in write
 * has to be changed.
 * Also, even if we ignore write operation for a moment, the value read from hash_table can not be cleared till Java has finished
 * reading it. Java will be responsible in this case to clear the memory (the memory occupied by the returned C char*). Means, after 
 * Java reads it, you need to expose another FFI method so thast Java can invoke it with input as the returned C char*;
 * and in C code this can be freed. This is not a neat approach. 
 * 
 * So instead, we are asking Java to pass a char* that C will fill and return. This ensures that memory for
 * returned json string is owned by Java (JVM) and C side does not have to deal with memory mgmnt for this variable. 
 * That is exactly what returnJsonBasicInfo is meant to do.
 * 
 * 
 */
void ffi_look_up_basic_customer_info(unsigned int custId, char* jsonString) {
    memset(jsonString, 0, CUSTOMER_INFO_JSON_STR_SIZE); //Java side is also doing it, but C should not depend on Java guy.
    
    unsigned int index = hash(custId);
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    
    customer_info *hashmap_value = look_up_cust_id(custId);

    if (hashmap_value != NULL) {
        char* firstName = hashmap_value->firstName;
        char* lastName = hashmap_value->lastName;
        char* phone = hashmap_value->phone;
        char* email = hashmap_value->email;
        double balance = hashmap_value->balance;

        /**
             * It is dangerous to return the original map value as it is to Java, as you need to unlock before returning.
             * Once unlocked, a write(insert_to_hash_table) for the same key may free the memory that is holding this structure. 
             * On Java end, as you try to get the value of the returned char pointer, it may get garbage, as the holder 
             * parent structure itself got collected (free'ed). 
             * 
             * So, you need to return a copy of the original map value to Java, and also inform Java guys to call the
             * corresponding free_string FFI method to free the memory used by this copied string. This is to be done once 
             * Java is done with reading the output from this method.
             * 
             * You wont need seperate free_string FFI call, if you use the char array passed by Java to fill in your final 
             * map value as a string.
             */

        snprintf(jsonString, CUSTOMER_INFO_JSON_STR_SIZE,
            "{"
            "\"id\":%d,"
            "\"firstName\":\"%s\","
            "\"lastName\":\"%s\","
            "\"phone\":\"%s\","
            "\"email\":\"%s\","
            "\"balance\":%.2f"
            "}",
            custId,
            firstName,
            lastName,
            phone,
            email,
            balance
            );       
    }   
    pthread_mutex_unlock(&bucket_locks[lock_index]);
}

/**
 * You may wonder why is there need to have a ffi_look_up_customer_balance, when there is already
 * a ffi_look_up_basic_customer_info. Now, the thing is, call to ffi_look_up_basic_customer_info
 * is costly, as it requires Java side to allocate memory for the input jsonString offheap.
 * Also, this offheap jsonString gets converted into new String(...) in the Java FFI implmn,
 * and then returned to the calling method. This also is another instance of alocating memory,
 * however, new String is executed on jvm heap and not offheap. So, we are hoping to reduce
 * instances of costly memory usage by directly returning double.
 * 
 * You may ask, even the FFI understood Java double might need to be converted into Java double ?
 * The answer is yes, but it happens automatically behind scenes. You dont need to write explicit 
 * code to convert C double to Java double, unlike the case of C char* to java String data type.
 * Yes, lets see how it goes.
 */
double ffi_look_up_customer_balance(unsigned int custId) {
    
    double returnBalance = -1;
    unsigned int index = hash(custId);
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    
    customer_info *hashmap_value = look_up_cust_id(custId);

    if (hashmap_value != NULL) {
        returnBalance =  hashmap_value->balance;  
    }   
    pthread_mutex_unlock(&bucket_locks[lock_index]);
    return returnBalance;
}

void ffi_addCustomerTransaction(int custId, long trxn_date, int opType, double amount)  {
    unsigned int index = hash(custId);
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    customer_info* customer_info = look_up_cust_id(custId);
    //printf("ffi_addCustomerTransaction: customer_info balance = %0.2f\n",customer_info->balance);

    if (customer_info == NULL)  {
        //Nothing to do here, as the customer with Id is not found in cache.
        return;
    }

    /*
        Find an empty index within the customer_info's latest_trxns. Empty/dummy indexes 
        are identified by trxnDate value = -1
    */
    int emptyIndex = getEmptyIndexInTransactionArrayOfCustomer(customer_info);
    if (opType == CREDIT_OP_TYPE) {
        double newBalance = customer_info->balance + amount;
        customer_info->balance = newBalance;
        transaction_details dummy =  {trxn_date, amount, 0.0, newBalance};
        customer_info->latest_trxns[emptyIndex] = dummy;
    }
    else if (opType == DEBIT_OP_TYPE) {
        if (customer_info->balance >= amount) {
            double newBalance = customer_info->balance - amount;
            customer_info->balance = newBalance;
            transaction_details dummy =  {trxn_date, 0.0, amount, newBalance};
            customer_info->latest_trxns[emptyIndex] = dummy;
        } 
    }
    pthread_mutex_unlock(&bucket_locks[lock_index]);
}



void ffi_look_up_customer_trxns(unsigned int custId, char* result) {
    memset(result, 0, CUSTOMER_TRXN_INFO_JSON_STR_SIZE); //Java side is also doing it, but C should not depend on Java guy.

    unsigned int index = hash(custId);
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    
    customer_info *hashmap_value = look_up_cust_id(custId);

    if (hashmap_value != NULL) {
        /**
         * Below assignment throws compilation error:
         *       transaction_details trxns[MAX_TRXNS_PER_CUSTOMER] = hashmap_value->latest_trxns;
         * 
         * src/implementation/caching-service.c:269:61: error: invalid initializer
           269 |  transaction_details trxns[MAX_TRXNS_PER_CUSTOMER] = hashmap_value->latest_trxns;

        You’re trying to assign an entire array (hashmap_value->latest_trxns) to another array (trxns).
        In C, you cannot assign arrays directly — neither in declarations nor later.

        That’s what this GCC error means.C arrays are not first-class objects; they don’t support assignment or copy semantics.
        
        Work directly on the original array
        If you don’t need a copy, and only want to access the existing array, just use a pointer.
        */

        transaction_details *trxns = hashmap_value->latest_trxns;

        char temp[130];//enough to hold 1 json transaction_details. 10 trxns occupy 1209 bytes.
        /**
         * ❗ temp is not automatically “cleared to zeros” before each use.
            It’s just overwritten completely by snprintf().
            If your formatted string is shorter than before, leftover garbage could remain beyond the new '\0',
            but it doesn’t matter — C strings always stop at the null terminator.
            And who puts null terminator to the string ? It is the job of snprintf.
         */
        snprintf(temp, sizeof(temp), "{\"id\":%d,\"trxns\":[", custId);
        strcat(result, temp);

        if (trxns[0].trxnDate != -1) {
                //not a dummy trxn entry
                snprintf(temp, sizeof(temp),
                    "{\"date\":%ld,\"credit\":%.2f,\"debit\":%.2f,\"balance\":%.2f}",
                    trxns[0].trxnDate, trxns[0].credit, trxns[0].debit, trxns[0].balance);
                strcat(result, temp);
        }

        for (int i = 1 ; i < MAX_TRXNS_PER_CUSTOMER ; i++){
            if (trxns[i].trxnDate != -1) {
                //not a dummy trxn entry
                snprintf(temp, sizeof(temp),
                    ",{\"date\":%ld,\"credit\":%.2f,\"debit\":%.2f,\"balance\":%.2f}",
                    trxns[i].trxnDate, trxns[i].credit, trxns[i].debit, trxns[i].balance);
                strcat(result, temp);
            }
        }

        strcat(result, "]}");  // close array and JSON object
    }   
    //printf("trxns = %s\n",result);
    pthread_mutex_unlock(&bucket_locks[lock_index]);
}




//end FFI methods
int main(int argc, char const *argv[]){
    ffi_init_hash_table();

    char firstName[MAX_NAME_CHARS] = "Binita";
    char lastName[MAX_NAME_CHARS] = "BHarati";
    char phone[MAX_PHONE_CHARS] = "984516786";
    char email[MAX_EMAIL_CHARS] = "binita@gmail.com";

    ffi_insert_to_hash_table(1, firstName, lastName, phone, email, 100.0);
    char jsonStr[CUSTOMER_INFO_JSON_STR_SIZE];
    ffi_look_up_basic_customer_info(1, jsonStr);
    printf("existing customer info = %s\n",jsonStr);

    char phone1[MAX_PHONE_CHARS] = "778877889933";
    char email1[MAX_EMAIL_CHARS] = "bharati@gmail.com";
    ffi_update_basic_customer_info(1, phone1, email1); 
    char jsonStr1[CUSTOMER_INFO_JSON_STR_SIZE];
    ffi_look_up_basic_customer_info(1, jsonStr1);
    printf("updated customer info = %s\n",jsonStr1);

    //ffi_addCustomerTransaction(1, 1234, 100.0, 0.0, 120.0);
    ffi_addCustomerTransaction(1, 1234, 0, 120.0);
    ffi_addCustomerTransaction(1, 3456, 1, 100.0);
    ffi_addCustomerTransaction(1, 2323, 0, 1200.0);
    ffi_addCustomerTransaction(1, 1212, 1, 200.0);
    ffi_addCustomerTransaction(1, 1213, 0, 1300.0);
    ffi_addCustomerTransaction(1, 3212, 0, 500.0);
    ffi_addCustomerTransaction(1, 3212, 1, 100.0);
    ffi_addCustomerTransaction(1, 2211, 0, 300.0);
    ffi_addCustomerTransaction(1, 2212, 1, 200.0);
    ffi_addCustomerTransaction(1, 3412, 0, 400.0);
    //11th trxn
    ffi_addCustomerTransaction(1, 4545, 0, 200.0);


    char result[CUSTOMER_TRXN_INFO_JSON_STR_SIZE];
    ffi_look_up_customer_trxns(1, result);
    printf("trxns = %s\n",result);

    return 0;

}