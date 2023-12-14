#ifndef JOT_PLATFORM
#define JOT_PLATFORM
#define _CRT_SECURE_NO_WARNINGS /* ... i hate msvc */

#include <stdint.h>
#include <limits.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

//This is a complete operating system abstarction layer. Its implementation is as stright forward and light as possible.
//It uses sized strings on all imputs and returns null terminated strings for maximum compatibility and performance.
//It tries to minimize the need to track state user side instead tries to operate on fixed ammount of mutable buffers.

//Why we need this:
//  1) Practical
//      The c standard library is extremely minimalistic so if we wish to list all files in a directory there is no other way.
// 
//  2) Idealogical
//     Its necessary to udnerstand the bedrock of any medium we are working with. Be it paper, oil & canvas or code, 
//     understanding the medium will help us define strong limitations on the final problem solutions. This is possible
//     and this isnt. Yes or no. This drastically shrinks the design space of any problem which allow for deeper exploration of it. 
//     
//     Interestingly it does not only shrink the design space it also makes it more defined. We see more oportunities that we 
//     wouldnt have seen if we just looked at some high level abstarction library. This can lead to development of better abstractions.
//  
//     Further having absolute control over the system is rewarding. Having the knowledge of every single operation that goes on is
//     immensely satisfying.

//=========================================
// Platform layer setup
//=========================================

//Initializes the platform layer interface. 
//Should be called before calling any other function.
void platform_init();

//Deinitializes the platform layer, freeing all allocated resources back to os.
//platform_init() should be called before using any other fucntion again!
void platform_deinit();

typedef struct Platform_Allocator {
    void* (*reallocate)(void* context, int64_t new_size, void* old_ptr, int64_t old_size);
    void* context;
} Platform_Allocator;

//Sets a different allocator used for internal allocations. This allocator must never fail (for the moment).
//The semantics must be quivalent to:
//   if(new_size == 0) return free(old_ptr);
//   else              return realloc(old_ptr, new_size);
//context is user defined argument and old_size is purely informative (can be used for tracking purposes).
//The value pointed to by context is not copied and needs to remain valid untill call to platform_deinit()!
void platform_set_internal_allocator(Platform_Allocator allocator);

//A non exhaustive list of operating systems
typedef enum Platform_Operating_System {
    PLATFORM_OS_UNKNOWN     = 0,
    PLATFORM_OS_WINDOWS     = 1,
    PLATFORM_OS_ANDROID     = 2,
    PLATFORM_OS_UNIX        = 3,
    PLATFORM_OS_BSD         = 4,
    PLATFORM_OS_APPLE_IOS   = 5,
    PLATFORM_OS_APPLE_OSX   = 6,
    PLATFORM_OS_SOLARIS     = 7,
    PLATFORM_OS_HP_UX       = 8,
    PLATFORM_OS_IBM_AIX     = 9,
} Platform_Operating_System;

typedef enum Platform_Endian {
    PLATFORM_ENDIAN_LITTLE = 0,
    PLATFORM_ENDIAN_BIG = 1,
    PLATFORM_ENDIAN_OTHER = 2, //We will never use this. But just for completion.
} Platform_Endian;

#ifndef PLATFORM_OS
    //Becomes one of the Platform_Operating_Systems based on the detected OS. (see below)
    //One possible use of this is to select the appropiate .c file for platform.h and include it
    // after main making the whole build unity build, greatly simpifying the build procedure.
    //Can be user overriden by defining it before including platform.h
    #define PLATFORM_OS          PLATFORM_OS_UNKNOWN                 
#endif

#ifndef PLATFORM_SYSTME_BITS
    //The adress space size of the system. Ie either 64 or 32 bit.
    //Can be user overriden by defining it before including platform.h
    #define PLATFORM_SYSTME_BITS ((UINTPTR_MAX == 0xffffffff) ? 32 : 64)
#endif

#ifndef PLATFORM_ENDIAN
    //The endianness of the system. Is by default PLATFORM_ENDIAN_LITTLE.
    //Can be user overriden by defining it before including platform.h
    #define PLATFORM_ENDIAN      PLATFORM_ENDIAN_LITTLE
#endif

#ifndef PLATFORM_MAX_ALIGN
    //Maximum alignment of bultin data type.
    //If this is incorrect (either too much or too little) please correct it by defining it!
    #define PLATFORM_MAX_ALIGN 16
#endif

//Can be used in files without including platform.h but still becomes
// valid if platform.h is included
#if PLATFORM_ENDIAN == PLATFORM_ENDIAN_LITTLE
    #define PLATFORM_HAS_ENDIAN_LITTLE PLATFORM_ENDIAN_LITTLE
#elif PLATFORM_ENDIAN == PLATFORM_ENDIAN_BIG
    #define PLATFORM_HAS_ENDIAN_BIG    PLATFORM_ENDIAN_BIG
#endif

//=========================================
// Virtual memory
//=========================================

typedef enum Platform_Virtual_Allocation {
    PLATFORM_VIRTUAL_ALLOC_RESERVE  = 0, //Reserves adress space so that no other allocation can be made there
    PLATFORM_VIRTUAL_ALLOC_COMMIT   = 1, //Commits adress space causing operating system to suply physical memory or swap file
    PLATFORM_VIRTUAL_ALLOC_DECOMMIT = 2, //Removes adress space from commited freeing physical memory
    PLATFORM_VIRTUAL_ALLOC_RELEASE  = 3, //Free adress space
} Platform_Virtual_Allocation;

typedef enum Platform_Memory_Protection {
    PLATFORM_MEMORY_PROT_NO_ACCESS  = 0,
    PLATFORM_MEMORY_PROT_READ       = 1,
    PLATFORM_MEMORY_PROT_WRITE      = 2,
    PLATFORM_MEMORY_PROT_READ_WRITE = 3,
} Platform_Memory_Protection;

void* platform_virtual_reallocate(void* allocate_at, int64_t bytes, Platform_Virtual_Allocation action, Platform_Memory_Protection protection);
void* platform_heap_reallocate(int64_t new_size, void* old_ptr, int64_t align);
//Returns the size in bytes of an allocated block. 
//old_ptr needs to be value returned from platform_heap_reallocate. Align must be the one supplied to platform_heap_reallocate.
//If old_ptr is NULL returns 0.
int64_t platform_heap_get_block_size(const void* old_ptr, int64_t align); 


//=========================================
// Errors 
//=========================================

typedef uint32_t Platform_Error;
enum {
    PLATFORM_ERROR_OK = 0, 
    //... errno codes
    PLATFORM_ERROR_OTHER = INT32_MAX, //Is used when the OS reports no error yet there was clearly an error.
};

//Returns a translated error message. The returned pointer is not static and shall NOT be stored as further calls to this functions will invalidate it. 
//Thus the returned string should be immedietelly printed or copied into a different buffer
const char* platform_translate_error(Platform_Error error);


//=========================================
// Threading
//=========================================

typedef struct Platform_Thread {
    void* handle;
    int32_t id;
} Platform_Thread;

typedef struct Platform_Mutex {
    void* handle;
} Platform_Mutex;

int64_t         platform_thread_get_proccessor_count();

//initializes a new thread and immedietely starts it with the func function.
//The thread has stack_size_or_zero bytes of stack sizes rounded up to page size
//If stack_size_or_zero is zero or lower uses system default stack size.
//The thread automatically cleans itself up upon completion or termination.
Platform_Error  platform_thread_launch(Platform_Thread* thread, void (*func)(void*), void* context, int64_t stack_size_or_zero); 

Platform_Thread platform_thread_get_current(); //Returns handle to the calling thread
void            platform_thread_sleep(int64_t ms); //Sleeps the calling thread for ms milliseconds
void            platform_thread_exit(int code); //Terminates a thread with an exit code
void            platform_thread_yield(); //Yields the remainder of this thread's time slice to the OS
Platform_Error  platform_thread_detach(Platform_Thread thread);
Platform_Error  platform_thread_join(const Platform_Thread* threads, int64_t count); //Blocks calling thread until all threads finish. Must not join the current calling thread!

Platform_Error  platform_mutex_init(Platform_Mutex* mutex);
void            platform_mutex_deinit(Platform_Mutex* mutex);
Platform_Error  platform_mutex_lock(Platform_Mutex* mutex);
void            platform_mutex_unlock(Platform_Mutex* mutex);


//=========================================
// Atomics 
//=========================================
inline static void platform_compiler_memory_fence();
inline static void platform_memory_fence();
inline static void platform_processor_pause();

//Returns the first/last set (1) bit position. If num is zero result is undefined.
//The follwing invarints hold (analogous for 64 bit)
// (num & (1 << platform_find_first_set_bit32(num)) != 0
// (num & (1 << (32 - platform_find_last_set_bit32(num))) != 0
inline static int32_t platform_find_first_set_bit32(uint32_t num);
inline static int32_t platform_find_first_set_bit64(uint64_t num);
inline static int32_t platform_find_last_set_bit32(uint32_t num); 
inline static int32_t platform_find_last_set_bit64(uint64_t num);

//Returns the number of set (1) bits 
inline static int32_t platform_pop_count32(uint32_t num);
inline static int32_t platform_pop_count64(uint64_t num);

//Standard Compare and Swap (CAS) semantics.
//Performs atomically: {
//   if(*target != old_value)
//      return false;
// 
//   *target = new_value;
//   return true;
// }
inline static bool platform_atomic_compare_and_swap64(volatile int64_t* target, int64_t old_value, int64_t new_value);
inline static bool platform_atomic_compare_and_swap32(volatile int32_t* target, int32_t old_value, int32_t new_value);

//Performs atomically: { return *target; }
inline static int64_t platform_atomic_load64(volatile const int64_t* target);
inline static int32_t platform_atomic_load32(volatile const int32_t* target);

//Performs atomically: { *target = value; }
inline static void platform_atomic_store64(volatile int64_t* target, int64_t value);
inline static void platform_atomic_store32(volatile int32_t* target, int32_t value);

//Performs atomically: { int64_t copy = *target; *target = value; return copy; }
inline static int64_t platform_atomic_excahnge64(volatile int64_t* target, int64_t value);
inline static int32_t platform_atomic_excahnge32(volatile int32_t* target, int32_t value);

//Performs atomically: { int64_t copy = *target; *target += value; return copy; }
inline static int32_t platform_atomic_add32(volatile int32_t* target, int32_t value);
inline static int64_t platform_atomic_add64(volatile int64_t* target, int64_t value);

//Performs atomically: { int64_t copy = *target; *target -= value; return copy; }
inline static int32_t platform_atomic_sub32(volatile int32_t* target, int32_t value);
inline static int64_t platform_atomic_sub64(volatile int64_t* target, int64_t value);

//=========================================
// Modifiers
//=========================================

//See below for implementation on each compiler.

#define MODIFIER_RESTRICT                                   /* C's restrict keyword. see: https://en.cppreference.com/w/c/language/restrict */
#define MODIFIER_FORCE_INLINE                               /* Ensures function will get inlined. Applied before function declartion. */
#define MODIFIER_NO_INLINE                                  /* Ensures function will not get inlined. Applied before function declartion. */
#define MODIFIER_THREAD_LOCAL                               /* Declares a variable thread local. Applied before variable declarition. */
#define MODIFIER_ALIGNED(bytes)                             /* Places a variable on the stack aligned to 'bytes' */
#define MODIFIER_FORMAT_FUNC(format_arg, format_arg_index)  /* Marks a function as formatting function. Applied before function declartion. See log.h for example */
#define MODIFIER_FORMAT_ARG                          /* Marks a format argument. Applied before const char* format argument. See log.h for example */  
#define MODIFIER_NORETURN                                   /* Specifices that this function will not return (for example abort, exit ...) . Applied before function declartion. */

//=========================================
// Timings
//=========================================

typedef struct Platform_Calendar_Time {
    int32_t year;       // any
    int8_t month;       // [0, 12)
    int8_t day_of_week; // [0, 7) where 0 is sunday
    int8_t day;         // [0, 31] !note the end bracket!
    
    int8_t hour;        // [0, 24)
    int8_t minute;      // [0, 60)
    int8_t second;      // [0, 60)
    
    int16_t millisecond; // [0, 1000)
    int16_t microsecond; // [0, 1000)
    //int16_t day_of_year; // [0, 365]
} Platform_Calendar_Time;

//returns the number of micro-seconds since the start of the epoch.
//This functions is very fast and suitable for fast profiling
int64_t platform_epoch_time();   
//returns the number of micro-seconds between the epoch and the call to platform_init()
int64_t platform_startup_epoch_time(); 

//converts the epoch time (micro second time since unix epoch) to calendar representation
Platform_Calendar_Time platform_calendar_time_from_epoch_time(int64_t epoch_time_usec);
//Converts calendar time to the precise epoch time (micro second time since unix epoch)
int64_t platform_epoch_time_from_calendar_time(Platform_Calendar_Time calendar_time);

Platform_Calendar_Time platform_local_calendar_time_from_epoch_time(int64_t epoch_time_usec);
int64_t platform_epoch_time_from_local_calendar_time(Platform_Calendar_Time calendar_time);

//Returns the current value of monotonic lowlevel performance counter. Is ideal for benchamrks.
//Generally is with nanosecond precisions.
int64_t platform_perf_counter();         
//returns the frequency of the performance counter (that is counter ticks per second)
int64_t platform_perf_counter_frequency();  
//returns platform_perf_counter() take at time of platform_init()
int64_t platform_perf_counter_startup();    

//=========================================
// Filesystem
//=========================================
typedef struct Platform_String {
    const char* data;
    int64_t size;
} Platform_String;

typedef enum Platform_File_Type {
    PLATFORM_FILE_TYPE_NOT_FOUND = 0,
    PLATFORM_FILE_TYPE_FILE = 1,
    PLATFORM_FILE_TYPE_DIRECTORY = 4,
    PLATFORM_FILE_TYPE_CHARACTER_DEVICE = 2,
    PLATFORM_FILE_TYPE_PIPE = 3,
    PLATFORM_FILE_TYPE_SOCKET = 5,
    PLATFORM_FILE_TYPE_OTHER = 6,
} Platform_File_Type;

typedef enum Platform_Link_Type {
    PLATFORM_LINK_TYPE_NOT_LINK = 0,
    PLATFORM_LINK_TYPE_HARD = 1,
    PLATFORM_LINK_TYPE_SOFT = 2,
    PLATFORM_LINK_TYPE_SYM = 3,
    PLATFORM_LINK_TYPE_OTHER = 4,
} Platform_Link_Type;

typedef struct Platform_File_Info {
    int64_t size;
    Platform_File_Type type;
    Platform_Link_Type link_type;
    int64_t created_epoch_time;
    int64_t last_write_epoch_time;  
    int64_t last_access_epoch_time; //The last time file was either read or written
} Platform_File_Info;
    
typedef struct Platform_Directory_Entry {
    char* path;
    int64_t index_within_directory;
    int64_t directory_depth;
    Platform_File_Info info;
} Platform_Directory_Entry;

typedef struct Platform_Memory_Mapping {
    void* address;
    int64_t size;
    uint64_t state[8];
} Platform_Memory_Mapping;

//retrieves info about the specified file or directory
Platform_Error platform_file_info(Platform_String file_path, Platform_File_Info* info_or_null);
//Creates an empty file at the specified path. Succeeds if the file exists after the call.
//Saves to was_just_created_or_null wheter the file was just now created. If is null doesnt save anything.
Platform_Error platform_file_create(Platform_String file_path, bool* was_just_created_or_null);
//Removes a file at the specified path. Succeeds if the file exists after the call the file does not exist.
//Saves to was_just_deleted_or_null wheter the file was just now deleted. If is null doesnt save anything.
Platform_Error platform_file_remove(Platform_String file_path, bool* was_just_deleted_or_null);
//Moves or renames a file. If the file cannot be found or renamed to file that already exists, fails.
Platform_Error platform_file_move(Platform_String new_path, Platform_String old_path);
//Copies a file. If the file cannot be found or copy_to_path file that already exists, fails.
Platform_Error platform_file_copy(Platform_String copy_to_path, Platform_String copy_from_path);

//Makes an empty directory
//Saves to was_just_created_or_null wheter the file was just now created. If is null doesnt save anything.
Platform_Error platform_directory_create(Platform_String dir_path, bool* was_just_created_or_null);
//Removes an empty directory
//Saves to was_just_deleted_or_null wheter the file was just now deleted. If is null doesnt save anything.
Platform_Error platform_directory_remove(Platform_String dir_path, bool* was_just_deleted_or_null);

//changes the current working directory to the new_working_dir.  
Platform_Error platform_directory_set_current_working(Platform_String new_working_dir);    
//Retrieves the absolute path current working directory
const char* platform_directory_get_current_working();    
//Retrieves the absolute path of the executable / dll
const char* platform_get_executable_path();    

//Gathers and allocates list of files in the specified directory. Saves a pointer to array of entries to entries and its size to entries_count. 
//Needs to be freed using directory_list_contents_free()
Platform_Error platform_directory_list_contents_alloc(Platform_String directory_path, Platform_Directory_Entry** entries, int64_t* entries_count, int64_t max_depth);
//Frees previously allocated file list
void platform_directory_list_contents_free(Platform_Directory_Entry* entries);

enum {
    PLATFORM_FILE_WATCH_CHANGE      = 1,
    PLATFORM_FILE_WATCH_DIR_NAME    = 2,
    PLATFORM_FILE_WATCH_FILE_NAME   = 4,
    PLATFORM_FILE_WATCH_ATTRIBUTES  = 8,
    PLATFORM_FILE_WATCH_RECURSIVE   = 16,
    PLATFORM_FILE_WATCH_ALL         = 31,
};

typedef struct Platform_File_Watch {
    Platform_Thread thread;
    void* handle;
} Platform_File_Watch;

//Creates a watch of a diretcory monitoring for events described in the file_watch_flags. 
//The async_func get called on another thread every time the appropriate action happens. This thread is in blocked state otherwise.
//If async_func returns false the file watch is closed and no further actions are reported.
Platform_Error platform_file_watch(Platform_File_Watch* file_watch, Platform_String dir_path, int32_t file_watch_flags, bool (*async_func)(void* context), void* context);
//Deinits the file watch stopping the monitoring thread.
void platform_file_unwatch(Platform_File_Watch* file_watch);

//Memory maps the file pointed to by file_path and saves the adress and size of the mapped block into mapping. 
//If the desired_size_or_zero == 0 maps the entire file. 
//  if the file doesnt exist the function fails.
//If the desired_size_or_zero > 0 maps only up to desired_size_or_zero bytes from the file.
//  The file is resized so that it is exactly desired_size_or_zero bytes (filling empty space with 0)
//  if the file doesnt exist the function creates a new file.
//If the desired_size_or_zero < 0 maps additional desired_size_or_zero bytes from the file 
//    (for appending) extending it by that ammount and filling the space with 0.
//  if the file doesnt exist the function creates a new file.
Platform_Error platform_file_memory_map(Platform_String file_path, int64_t desired_size_or_zero, Platform_Memory_Mapping* mapping);
//Unmpas the previously mapped file. If mapping is a result of failed platform_file_memory_map does nothing.
void platform_file_memory_unmap(Platform_Memory_Mapping* mapping);

//=========================================
// Window managmenet
//=========================================

typedef enum Platform_Window_Popup_Style
{
    PLATFORM_POPUP_STYLE_OK = 0,
    PLATFORM_POPUP_STYLE_ERROR,
    PLATFORM_POPUP_STYLE_WARNING,
    PLATFORM_POPUP_STYLE_INFO,
    PLATFORM_POPUP_STYLE_RETRY_ABORT,
    PLATFORM_POPUP_STYLE_YES_NO,
    PLATFORM_POPUP_STYLE_YES_NO_CANCEL,
} Platform_Window_Popup_Style;

typedef enum Platform_Window_Popup_Controls
{
    PLATFORM_POPUP_CONTROL_OK,
    PLATFORM_POPUP_CONTROL_CANCEL,
    PLATFORM_POPUP_CONTROL_CONTINUE,
    PLATFORM_POPUP_CONTROL_ABORT,
    PLATFORM_POPUP_CONTROL_RETRY,
    PLATFORM_POPUP_CONTROL_YES,
    PLATFORM_POPUP_CONTROL_NO,
    PLATFORM_POPUP_CONTROL_IGNORE,
} Platform_Window_Popup_Controls;

//Makes default shell popup with a custom message and style
Platform_Window_Popup_Controls  platform_window_make_popup(Platform_Window_Popup_Style desired_style, Platform_String message, Platform_String title);

//=========================================
// Debug
//=========================================
//Could be separate file or project from here on...

typedef struct {
    char function[256]; //mangled or unmangled function name
    char module[256];   //mangled or unmangled module name ie. name of dll/executable
    char file[256];     //file or empty if not supported
    int64_t line;       //0 if not supported;
    void* address;
} Platform_Stack_Trace_Entry;

//Stops the debugger at the call site
#define platform_trap() (*(char*)0 = 0)
//Marks a piece of code as unreachable for the compiler
#define platform_assume_unreachable() (*(char*)0 = 0)

//Captures the current stack frame pointers. 
//Saves up to stack_size pointres into the stack array and returns the number of
//stack frames captures. If the returned number is exactly stack_size a bigger buffer MIGHT be required.
//Skips first skip_count stack pointers from the position of the called. 
//(even with skip_count = 0 platform_capture_call_stack() will not be included within the stack)
int64_t platform_capture_call_stack(void** stack, int64_t stack_size, int64_t skip_count);

//Translates captured stack into helpful entries. Operates on fixed width strings to guarantee this function
//will never fail yet translate all needed stack frames. 
void platform_translate_call_stack(Platform_Stack_Trace_Entry* translated, const void** stack, int64_t stack_size);

typedef enum Platform_Exception {
    PLATFORM_EXCEPTION_NONE = 0,
    PLATFORM_EXCEPTION_ACCESS_VIOLATION,
    PLATFORM_EXCEPTION_DATATYPE_MISALIGNMENT,
    PLATFORM_EXCEPTION_FLOAT_DENORMAL_OPERAND,
    PLATFORM_EXCEPTION_FLOAT_DIVIDE_BY_ZERO,
    PLATFORM_EXCEPTION_FLOAT_INEXACT_RESULT,
    PLATFORM_EXCEPTION_FLOAT_INVALID_OPERATION,
    PLATFORM_EXCEPTION_FLOAT_OVERFLOW,
    PLATFORM_EXCEPTION_FLOAT_UNDERFLOW,
    PLATFORM_EXCEPTION_FLOAT_OTHER,
    PLATFORM_EXCEPTION_PAGE_ERROR,
    PLATFORM_EXCEPTION_INT_DIVIDE_BY_ZERO,
    PLATFORM_EXCEPTION_INT_OVERFLOW,
    PLATFORM_EXCEPTION_ILLEGAL_INSTRUCTION,
    PLATFORM_EXCEPTION_PRIVILAGED_INSTRUCTION,
    PLATFORM_EXCEPTION_BREAKPOINT,
    PLATFORM_EXCEPTION_BREAKPOINT_SINGLE_STEP,
    PLATFORM_EXCEPTION_STACK_OVERFLOW, //cannot be caught inside error_func because of obvious reasons
    PLATFORM_EXCEPTION_ABORT,
    PLATFORM_EXCEPTION_TERMINATE = 0x0001000,
    PLATFORM_EXCEPTION_OTHER = 0x0001001,
} Platform_Exception; 

typedef struct Platform_Sandbox_Error {
    //The exception that occured
    Platform_Exception exception;
    
    //A translated stack trace and its size
    const Platform_Stack_Trace_Entry* call_stack; 
    int64_t call_stack_size;

    //Platform specific data containing the cpu state and its size (so that it can be copied and saved)
    const void* execution_context;
    int64_t execution_context_size;

    //The epoch time of the exception and offset in nanoseconds to the exact time 
    int64_t epoch_time;
    int64_t nanosec_offset;
} Platform_Sandbox_Error;

//Launches the sandboxed_func inside a sendbox protecting the outside environment 
// from any exceptions, including hardware exceptions that might occur inside sandboxed_func.
//If an exception occurs collects execution context including stack pointers and gracefuly recovers. 
//Returns the error that occured or PLATFORM_EXCEPTION_NONE = 0 on success.
Platform_Exception platform_exception_sandbox(
    void (*sandboxed_func)(void* sandbox_context),   
    void* sandbox_context,
    void (*error_func)(void* error_context, Platform_Sandbox_Error error),
    void* error_context);

//Convertes the sandbox error to string. The string value is the name of the enum
// (PLATFORM_EXCEPTION_ACCESS_VIOLATION -> "PLATFORM_EXCEPTION_ACCESS_VIOLATION")
const char* platform_exception_to_string(Platform_Exception error);

#if PLATFORM_OS == PLATFORM_OS_UNKNOWN
    #undef PLATFORM_OS
    #if defined(_WIN32)
        #define PLATFORM_OS PLATFORM_OS_WINDOWS // Windows
    #elif defined(_WIN64)
        #define PLATFORM_OS PLATFORM_OS_WINDOWS // Windows
    #elif defined(__CYGWIN__) && !defined(_WIN32)
        #define PLATFORM_OS PLATFORM_OS_WINDOWS // Windows (Cygwin POSIX under Microsoft Window)
    #elif defined(__ANDROID__)
        #define PLATFORM_OS PLATFORM_OS_ANDROID // Android (implies Linux, so it must come first)
    #elif defined(__linux__)
        #define PLATFORM_OS "linux" // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
    #elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
        #include <sys/param.h>
        #if defined(BSD)
            #define PLATFORM_OS PLATFORM_OS_BSD // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
        #endif
    #elif defined(__hpux)
        #define PLATFORM_OS PLATFORM_OS_HP_UX // HP-UX
    #elif defined(_AIX)
        #define PLATFORM_OS PLATFORM_OS_IBM_AIX // IBM AIX
    #elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)
        #include <TargetConditionals.h>
        #if TARGET_IPHONE_SIMULATOR == 1
            #define PLATFORM_OS PLATFORM_OS_APPLE_IOS // Apple iOS
        #elif TARGET_OS_IPHONE == 1
            #define PLATFORM_OS PLATFORM_OS_APPLE_IOS // Apple iOS
        #elif TARGET_OS_MAC == 1
            #define PLATFORM_OS PLATFORM_OS_APPLE_OSX // Apple OSX
        #endif
    #elif defined(__sun) && defined(__SVR4)
        #define PLATFORM_OS PLATFORM_OS_SOLARIS // Oracle Solaris, Open Indiana
    #else
        #define PLATFORM_OS PLATFORM_OS_UNKNOWN
    #endif
#endif 

#undef MODIFIER_RESTRICT                                   
#undef MODIFIER_FORCE_INLINE                               
#undef MODIFIER_NO_INLINE                                  
#undef MODIFIER_THREAD_LOCAL                               
#undef MODIFIER_ALIGNED                            
#undef MODIFIER_FORMAT_FUNC 
#undef MODIFIER_FORMAT_ARG                          
#undef MODIFIER_NORETURN                                   

// =================== INLINE IMPLEMENTATION ============================
#if defined(_MSC_VER)
    #include <stdio.h>
    #include <intrin.h>
    #include <assert.h>
    #include <sal.h> //for _Printf_format_string_

    #undef platform_trap
    #define platform_trap() __debugbreak() 

    #undef platform_assume_unreachable
    #define platform_assume_unreachable() __assume(0)

    #define MODIFIER_RESTRICT                                                 __restrict
    #define MODIFIER_FORCE_INLINE                                             __forceinline
    #define MODIFIER_NO_INLINE                                                __declspec(noinline)
    #define MODIFIER_THREAD_LOCAL                                             __declspec(thread)
    #define MODIFIER_ALIGNED(bytes)                                           __declspec(align(bytes))
    #define MODIFIER_FORMAT_FUNC(format_arg, format_arg_index)                /* empty */
    #define MODIFIER_FORMAT_ARG                                               _Printf_format_string_  

    inline static void platform_compiler_memory_fence() 
    {
        _ReadWriteBarrier();
    }

    inline static void platform_memory_fence()
    {
        _ReadWriteBarrier(); 
        __faststorefence();
    }

    inline static void platform_processor_pause()
    {
        _mm_pause();
    }
    
    inline static int32_t platform_find_last_set_bit32(uint32_t num)
    {
        assert(num != 0);
        unsigned long out = 0;
        _BitScanReverse(&out, (unsigned long) num);
        return (int32_t) out;
    }
    
    inline static int32_t platform_find_last_set_bit64(uint64_t num)
    {
        assert(num != 0);
        unsigned long out = 0;
        _BitScanReverse64(&out, (unsigned long long) num);
        return (int32_t) out;
    }

    inline static int32_t platform_find_first_set_bit32(uint32_t num)
    {
        assert(num != 0);
        unsigned long out = 0;
        _BitScanForward(&out, (unsigned long) num);
        return (int32_t) out;
    }
    inline static int32_t platform_find_first_set_bit64(uint64_t num)
    {
        assert(num != 0);
        unsigned long out = 0;
        _BitScanForward64(&out, (unsigned long long) num);
        return (int32_t) out;
    }
    
    inline static int32_t platform_pop_count32(uint32_t num)
    {
        return (int32_t) __popcnt((unsigned int) num);
    }
    inline static int32_t platform_pop_count64(uint64_t num)
    {
        return (int32_t) __popcnt64((unsigned __int64)num);
    }
    
    inline static int64_t platform_atomic_load64(volatile const int64_t* target)
    {
        return (int64_t) _InterlockedOr64((volatile long long*) target, 0);
    }
    inline static int32_t platform_atomic_load32(volatile const int32_t* target)
    {
        return (int32_t) _InterlockedOr((volatile long*) target, 0);
    }

    inline static void platform_atomic_store64(volatile int64_t* target, int64_t value)
    {
        platform_atomic_excahnge64(target, value);
    }
    inline static void platform_atomic_store32(volatile int32_t* target, int32_t value)
    {
        platform_atomic_excahnge32(target, value);
    }

    inline static bool platform_atomic_compare_and_swap64(volatile int64_t* target, int64_t old_value, int64_t new_value)
    {
        return _InterlockedCompareExchange64((volatile long long*) target, (long long) new_value, (long long) old_value) == (long long) old_value;
    }

    inline static bool platform_atomic_compare_and_swap32(volatile int32_t* target, int32_t old_value, int32_t new_value)
    {
        return _InterlockedCompareExchange((volatile long*) target, (long) new_value, (long) old_value) == (long) old_value;
    }

    inline static int64_t platform_atomic_excahnge64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) _InterlockedExchange64((volatile long long*) target, (long long) value);
    }

    inline static int32_t platform_atomic_excahnge32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) _InterlockedExchange((volatile long*) target, (long) value);
    }
    
    inline static int64_t platform_atomic_add64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) _InterlockedExchangeAdd64((volatile long long*) target, (long long) value);
    }

    inline static int32_t platform_atomic_add32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) _InterlockedExchangeAdd((volatile long*) target, (long) value);
    }

    inline static int32_t platform_atomic_sub32(volatile int32_t* target, int32_t value)
    {
        return platform_atomic_add32(target, -value);
    }

    inline static int64_t platform_atomic_sub64(volatile int64_t* target, int64_t value)
    {
        return platform_atomic_add64(target, -value);
    }
   
#elif defined(__GNUC__) || defined(__clang__)

    #include <signal.h>

    #undef platform_trap
    // #define platform_trap() __builtin_trap() /* bad looks like a fault in program! */
    #define platform_trap() raise(SIGTRAP)
    
    #undef platform_assume_unreachable
    #define platform_assume_unreachable()                                    __builtin_unreachable() /*move to platform! */

    #define MODIFIER_RESTRICT                                                __restrict__
    #define MODIFIER_FORCE_INLINE                                            __attribute__((always_inline))
    #define MODIFIER_NO_INLINE                                               __attribute__((noinline))
    #define MODIFIER_THREAD_LOCAL                                            __thread
    #define MODIFIER_ALIGNED(bytes)                                          __attribute__((aligned(bytes)))
    #define MODIFIER_FORMAT_FUNC(format_arg, format_arg_index)               __attribute__((format_arg (printf, format_arg_index, 0)))
    #define MODIFIER_FORMAT_ARG                                      /* empty */    
    #define MODIFIER_NORETURN                                               __attribute__((noreturn))

    typedef __MAX_ALIGN_TESTER__ char[
        __alignof__(long long int) == PLATFORM_MAX_ALIGN || 
        __alignof__(long double) == PLATFORM_MAX_ALIGN ? 1 : -1
    ];

    inline static void platform_compiler_memory_fence() 
    {
        __asm__ __volatile__("":::"memory");
    }

    inline static void platform_memory_fence()
    {
        platform_compiler_memory_fence(); 
        __sync_synchronize();
    }

    #if defined(__x86_64__) || defined(__i386__)
        #include <immintrin.h> // For _mm_pause
        inline static void platform_processor_pause()
        {
            _mm_pause();
        }
    #else
        #include <time.h>
        inline static void platform_processor_pause()
        {
            struct timespec spec = {0};
            spec.tv_sec = 0;
            spec.tv_nsec = 1;
            nanosleep(spec, NULL);
        }
    #endif

    //for refernce see: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
    inline static int32_t platform_find_last_set_bit32(uint32_t num)
    {
        return __builtin_ffs((int) num) - 1;
    }
    inline static int32_t platform_find_last_set_bit64(uint64_t num)
    {
        return __builtin_ffsll((long long) num) - 1;
    }

    inline static int32_t platform_find_first_set_bit32(uint32_t num)
    {
        return 32 - __builtin_ctz((unsigned int) num) - 1;
    }
    inline static int32_t platform_find_first_set_bit64(uint64_t num)
    {
        return 64 - __builtin_ctzll((unsigned long long) num) - 1;
    }

    inline static int32_t platform_pop_count32(uint32_t num)
    {
        return __builtin_popcount((uint32_t) num);
    }
    inline static int32_t platform_pop_count64(uint64_t num)
    {
        return __builtin_popcountll((uint64_t) num);
    }

    //for reference see: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    inline static bool platform_atomic_compare_and_swap64(volatile int64_t* target, int64_t old_value, int64_t new_value)
    {
        return __atomic_compare_exchange_n(target, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    
    inline static bool platform_atomic_compare_and_swap32(volatile int32_t* target, int32_t old_value, int32_t new_value)
    {
        return __atomic_compare_exchange_n(target, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

    inline static int64_t platform_atomic_load64(volatile const int64_t* target)
    {
        return (int64_t) __atomic_load_n(target, __ATOMIC_SEQ_CST);
    }
    inline static int32_t platform_atomic_load32(volatile const int32_t* target)
    {
        return (int32_t) __atomic_load_n(target, __ATOMIC_SEQ_CST);
    }

    inline static void platform_atomic_store64(volatile int64_t* target, int64_t value)
    {
        __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
    }
    inline static void platform_atomic_store32(volatile int32_t* target, int32_t value)
    {
        __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
    }

    inline static int64_t platform_atomic_excahnge64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
    }
    inline static int32_t platform_atomic_excahnge32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
    }

    inline static int32_t platform_atomic_add32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) __atomic_add_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    inline static int64_t platform_atomic_add64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) __atomic_add_fetch(target, value, __ATOMIC_SEQ_CST);
    }

    inline static int32_t platform_atomic_sub32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) __atomic_sub_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    inline static int64_t platform_atomic_sub64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) __atomic_sub_fetch(target, value, __ATOMIC_SEQ_CST);
    }


#endif

#endif
