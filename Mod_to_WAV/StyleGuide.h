#pragma once

/*
*******************************************************************************
*******************************************************************************
**                                                                           **
**        BASIC STYLING                                                      **
**                                                                           **
*******************************************************************************
*******************************************************************************
*/

class MyClass 
{
public:
    MyClass() {}

private:
    void    privateFunction();
    void    smallPrivateFunction()
    {
        privateVariable_ = 25;
    }



private:
    int     privateVariable_;

    
};

void MyClass::privateFunction()
{
    //milliseconds s = 0;
}

void main_function()
{

}



/*
    This file serves as a style guide for this (and maybe future other) 
    projects.
    Source: http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
    I've taken what I could understand, and tried to rewrite it / take from it
    whatever I found useful.


*******************************************************************************
*******************************************************************************
**                                                                           **
**        PHILOSOPHY                                                         **
**                                                                           **
*******************************************************************************
*******************************************************************************


===============================================================================
P.1: Express ideas directly in code
===============================================================================

    A well-designed library expresses intent (what is to be done, rather than 
    just how something is being done) far better than direct use of language 
    features.


    class Date {
        // ...
    public:
        Month month() const;  // do
        int month();          // don't
        // ...
    };

===============================================================================
P.2: Write in ISO Standard C++
===============================================================================

===============================================================================
P.3: Express intent
===============================================================================

    Alternative formulation: Say what should be done, rather than just how it 
    should be done. Some language constructs express intent better than others.

    draw_line(int, int, int, int);  // obscure
    draw_line(Point, Point);        // clearer

    // bad:
    gsl::index i = 0;
    while (i < v.size()) {
    // ... do something with v[i] ...
    }

    // better
    for (const auto& x : v) { ... do something with the value of x ... }

===============================================================================
P.4: Ideally, a program should be statically type safe
===============================================================================

    We can ban, restrain, or detect the individual problem categories 
    separately, as required and feasible for individual programs. Always 
    suggest an alternative. For example:
        unions – use variant (in C++17)
        casts – minimize their use; templates can help
        array decay – use span (from the GSL)
        range errors – use span
        narrowing conversions – minimize their use and use narrow or narrow_cast 
                                        (from the GSL) where they are necessary

===============================================================================
P.5: Prefer compile-time checking to run-time checking
===============================================================================

    Don’t postpone to run time what can be done well at compile time.


    // bad:
    void read(int* p, int n);   // read max n integers into *p
    // ...
    int a[100];
    read(a, 1000);              // bad, off the end


    // better:
    void read(span<int> r);     // read into the range of integers r
    // ...
    int a[100];
    read(a);     // better: let the compiler figure out the number of elements

===============================================================================
P.6: What cannot be checked at compile time should be checkable at run time
===============================================================================

===============================================================================
P.7: Catch run-time errors early
===============================================================================

    Look at pointers and arrays: Do range-checking early and not repeatedly
    Look at conversions: Eliminate or mark narrowing conversions
    Look for unchecked values coming from input
    Look for structured data (objects of classes with invariants) being 
    converted into strings

===============================================================================
P.8: Don’t leak any resources
===============================================================================

    A leak is colloquially “anything that isn’t cleaned up.” The more 
    important classification is “anything that can no longer be cleaned up.”
    For example, allocating an object on the heap and then losing the last 
    pointer that points to that allocation. This rule should not be taken as 
    requiring that allocations within long-lived objects must be returned 
    during program shutdown. For example, relying on system guaranteed cleanup 
    such as file closing and memory deallocation upon process shutdown can 
    simplify code. However, relying on abstractions that implicitly clean up 
    can be as simple, and often safer.

===============================================================================
P.9: Don’t waste time or space
===============================================================================

    // bad:
    void lower(zstring s)
    {
        for (int i = 0; i < strlen(s); ++i) s[i] = tolower(s[i]);
    }

===============================================================================
P.10: Prefer immutable data to mutable data
===============================================================================

    It is easier to reason about constants than about variables. Something 
    immutable cannot change unexpectedly. Sometimes immutability enables better 
    optimization. You can’t have a data race on a constant.

===============================================================================
P.11: Encapsulate messy constructs, rather than spreading through the code
===============================================================================

    Messy code is more likely to hide bugs and harder to write. A good 
    interface is easier and safer to use. Messy, low-level code breeds more 
    such code.

===============================================================================
P.12: Use supporting tools as appropriate
===============================================================================

    Reason: There are many things that are done better “by machine”. Computers 
    don’t tire or get bored by repetitive tasks. We typically have better 
    things to do than repeatedly do routine tasks. 
    Example: Run a static analyzer to verify that your code follows the 
    guidelines you want it to follow.

===============================================================================
P.13: Use support libraries as appropriate
===============================================================================

    Unless you are an expert in sorting algorithms and have plenty of time, 
    this is more likely to be correct and to run faster than anything you write 
    for a specific application. You need a reason not to use the standard 
    library (or whatever foundational libraries your application uses) rather 
    than a reason to use it.

    If no well-designed, well-documented, and well-supported library exists for 
    an important domain, maybe you should design and implement it, and then use 
    it.


*******************************************************************************
*******************************************************************************
**                                                                           **
**        INTERFACES                                                         **
**                                                                           **
*******************************************************************************
*******************************************************************************

    An interface is a contract between two parts of a program. Precisely 
    stating what is expected of a supplier of a service and a user of that 
    service is essential. Having good (easy-to-understand, encouraging 
    efficient use, not error-prone, supporting testing, etc.) interfaces is 
    probably the most important single aspect of code organization.


===============================================================================
I.1: Make interfaces explicit
===============================================================================

    Example, bad: Controlling the behavior of a function through a global 
    (namespace scope) variable (a call mode) is implicit and potentially 
    confusing. For example:

    int round(double d)
    {
        return (round_up) ? ceil(d) : d;    // don't: "invisible" dependency
    }

    It will not be obvious to a caller that the meaning of two calls of 
    round(7.2) might give different results.

===============================================================================
I.2: Avoid non-const global variables
===============================================================================

    Exception: cin, cout, cerr
    Note: gobal const is useful

===============================================================================
I.3: Avoid singletons
===============================================================================

    Singletons are basically complicated global objects in disguise.
    If you don’t want a global object to change, declare it const or constexpr.

===============================================================================
I.4: Make interfaces precisely and strongly typed
===============================================================================

    Types are the simplest and best documentation, improve legibility due to 
    their well-defined meaning, and are checked at compile time. Also, 
    precisely typed code is often optimized better.

    Example:
    void draw_rectangle(Point top_left, Point bottom_right);
    void draw_rectangle(Point top_left, Size height_width);

===============================================================================
I.5: State preconditions (if any)
===============================================================================

    Example:
    double sqrt(double x); // x must be nonnegative
    double sqrt(double x) { Expects(x >= 0); ... }

===============================================================================
I.6: Prefer Expects() for expressing preconditions
===============================================================================

    To make it clear that the condition is a precondition and to enable tool 
    use. 

    Example:
    int area(int height, int width)
    {
        Expects(height > 0 && width > 0);            // good
        if (height <= 0 || width <= 0) my_error();   // obscure
        // ...
    }

    No, using unsigned is not a good way to sidestep the problem of ensuring 
    that a value is nonnegative.

===============================================================================
I.7: State postconditions
===============================================================================

    Postconditions are often informally stated in a comment that states the 
    purpose of a function; Ensures() can be used to make this more systematic, 
    visible, and checkable. 
    Postconditions are especially important when they relate to something that 
    is not directly reflected in a returned result, such as a state of a data 
    structure used.

    Example:
    void manipulate(Record& r)    // postcondition: m is unlocked upon exit
    {
        m.lock();
        // ... no m.unlock() ... <--- bug is obvious
    }

===============================================================================
I.8: Prefer Ensures() for expressing postconditions
===============================================================================

    To make it clear that the condition is a postcondition and to enable tool 
    use. 

    Example:
    void f()
    {
        char buffer[MAX];
        // ...
        memset(buffer, 0, MAX);
        Ensures(buffer[0] == 0);
    }

===============================================================================
I.9: If an interface is a template, document its parameters using concepts
===============================================================================

===============================================================================
I.10: Use exceptions to signal a failure to perform a required task
===============================================================================

    What is an error? An error means that the function cannot achieve its 
    advertised purpose (including establishing postconditions). Calling code 
    that ignores an error could lead to wrong results or undefined systems 
    state. For example, not being able to connect to a remote server is not by 
    itself an error: the server can refuse a connection for all kinds of 
    reasons, so the natural thing is to return a result that the caller should 
    always check. However, if failing to make a connection is considered an 
    error, then a failure should throw an exception.

===============================================================================
I.11: Never transfer ownership by a raw pointer (T*) or reference (T&)
===============================================================================

    If there is any doubt whether the caller or the callee owns an object, 
    leaks or premature destruction will occur.

    Example. Consider:
    X* compute(args)    // don't
    {
        X* res = new X{};
        // ...
        return res;
    }

    Who deletes the returned X? The problem would be harder to spot if compute 
    returned a reference. Consider returning the result by value (use move 
    semantics if the result is large):

    vector<double> compute(args)  // good
    {
        vector<double> res(10000);
        // ...
        return res;
    }

    Note: Every object passed as a raw pointer (or iterator) is assumed to be 
    owned by the caller, so that its lifetime is handled by the caller. Viewed 
    another way: ownership transferring APIs are relatively rare compared to 
    pointer-passing APIs, so the default is “no ownership transfer.”

===============================================================================
I.12: Declare a pointer that must not be null as not_null
===============================================================================

===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================
===============================================================================





















*/
