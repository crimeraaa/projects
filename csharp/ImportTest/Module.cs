namespace Module;

using System;

/// <summary> Create via `dotnet new class` in the desired caller's directory.</summary>
public class Test {
    /// <summary>
    ///     Prompts user to enter a string of text.
    /// </summary> 
    /// <param name="prompt">
    ///     String to indicate user needs to input something. No newline printed.
    /// </param> 
    /// <returns>
    ///     Their line of input or "(null)" on error.
    /// </returns>
    public static string GetString(string prompt) {
        Console.Write(prompt); // Don't append newline so prompt looks nice
        string? input = Console.ReadLine();
        return (input != null) ? input : "(null)";
    }

    /// <summary>
    ///     Call `Imported.Test.Write();` to see if this works!
    /// </summary>
    /// <remarks>
    ///     Note that the module file must be in the same project directory
    ///     as the caller. 
    /// </remarks>     
    public static void Write() {
        Console.WriteLine("Hello from Module.cs!");
    }
}
